#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <Preferences.h>
Preferences preferences;

using namespace std;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t buttonHandle = NULL;

#define CLK 34
#define DT 39
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SELECT_BUTTON 27
#define BACK_BUTTON 14

#define LED_PIN 23
#define LED_COUNT 14
#define PIEZO 2

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
bool usePreset1 = true;
volatile int lastStateCLK = LOW;
volatile int delta{0};

// main menu
std::atomic<int> selectedMainIndex{0};
constexpr int menuItemCount = 2;
const char *mainMenuItems[menuItemCount] = {
    "Base Color",
    "Hit Color"};

// preset menu
std::atomic<int> selectedPresetIndex{0};
constexpr int presetMenuItemCount = 3;
const char *presetMenuItems[presetMenuItemCount] = {
    "Preset 1",
    "Preset 2",
    "Preset 3",
};

// rgbb menu
std::atomic<int> selectedRGBBIndex{0};
constexpr int RGBBMenuItemCount = 4;
const char *RGBBMenuItems[RGBBMenuItemCount] = {
    "Red Channel",
    "Blue Channel",
    "Green Channel",
    "Brightness"};

atomic<uint8_t> counter{0};
enum MenuState
{
  MENU_MAIN,
  PRESET_SELECT,
  RGBB_SCREEN,
  NUMBER_SCREEN
};
std::atomic<MenuState> currentMenu{MENU_MAIN};
atomic<uint8_t> saveColor[menuItemCount][presetMenuItemCount][RGBBMenuItemCount];
void menu_screen();
void preset_screen();
void rgbb_screen();
void number_screen();
void printSaveColors();

void saveToNVS()
{
  preferences.begin("ledprefs", false); // namespace "ledprefs", read/write
  for (int m = 0; m < menuItemCount; m++)
  {
    for (int p = 0; p < presetMenuItemCount; p++)
    {
      for (int c = 0; c < RGBBMenuItemCount; c++)
      {
        String key = "c_" + String(m) + "_" + String(p) + "_" + String(c);
        preferences.putUChar(key.c_str(), saveColor[m][p][c].load());
      }
    }
  }
  preferences.end();
}
void loadFromNVS()
{
  preferences.begin("ledprefs", true); // read-only
  for (int m = 0; m < menuItemCount; m++)
  {
    for (int p = 0; p < presetMenuItemCount; p++)
    {
      for (int c = 0; c < RGBBMenuItemCount; c++)
      {
        String key = "c_" + String(m) + "_" + String(p) + "_" + String(c);
        saveColor[m][p][c].store(preferences.getUChar(key.c_str(), 0)); // default to 0
      }
    }
  }
  preferences.end();
}

void IRAM_ATTR updateEncoder()
{
  int currentStateCLK = digitalRead(CLK);
  if (currentStateCLK != lastStateCLK)
  {
    delta = (digitalRead(DT) != currentStateCLK) ? 1 : -1;

    if (currentMenu.load() == MENU_MAIN)
    {
      int newIndex = selectedMainIndex.load() + delta;
      if (newIndex > menuItemCount - 1)
        newIndex = menuItemCount - 1;
      if (newIndex < 0)
        newIndex = 0;
      selectedMainIndex.store(newIndex);
    }

    if (currentMenu.load() == PRESET_SELECT)
    {
      int newIndex = selectedPresetIndex.load() + delta;
      if (newIndex > presetMenuItemCount - 1)
        newIndex = presetMenuItemCount - 1;
      if (newIndex < 0)
        newIndex = 0;
      selectedPresetIndex.store(newIndex);
    }
    if (currentMenu.load() == RGBB_SCREEN)
    {
      int newIndex = selectedRGBBIndex.load() + delta;
      if (newIndex > RGBBMenuItemCount - 1)
        newIndex = RGBBMenuItemCount - 1;
      if (newIndex < 0)
        newIndex = 0;
      selectedRGBBIndex.store(newIndex);
    }
    if (currentMenu.load() == NUMBER_SCREEN)
    {
      counter.store(counter.load() + delta);
    }
  }
  lastStateCLK = currentStateCLK;
}

void rainbow(uint8_t wait)
{
  for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256)
  {
    for (int i = 0; i < strip.numPixels(); i++)
    {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(wait);
  }
}
void snakeTail(uint8_t tailLength, uint8_t speed)
{
  static int pos = 0;

  for (int i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, 0);
  }

  for (int i = 0; i < tailLength; i++)
  {
    int index = (pos - i + strip.numPixels()) % strip.numPixels();
    uint8_t brightness = map(i, 0, tailLength - 1, 255, 50);
    strip.setPixelColor(index, brightness, brightness, 0);
  }

  strip.show();
  pos = (pos + 1) % strip.numPixels();
  delay(speed);
}

void displayTask(void *pvParameters)
{
  while (true)
  {
    if (currentMenu.load() == MENU_MAIN)
      menu_screen();
    else if (currentMenu.load() == PRESET_SELECT)
      preset_screen();
    else if (currentMenu.load() == RGBB_SCREEN)
      rgbb_screen();
    else if (currentMenu.load() == NUMBER_SCREEN)
      number_screen();

    vTaskDelay(pdMS_TO_TICKS(1)); // Refresh every 100ms
  }
}
void ledTask(void *pvParameters)
{
  uint16_t potValues[8]; // Simulated

  while (true)
  {
    usePreset1 = digitalRead(PIEZO) ? 1 : 0;

    if (usePreset1)
    {
      // Simulated values
      uint8_t r = map(potValues[0], 0, 1023, 0, 255);
      uint8_t g = map(potValues[1], 0, 1023, 0, 255);
      uint8_t b = map(potValues[2], 0, 1023, 0, 255);
      uint8_t brightness = map(potValues[6], 0, 1023, 0, 255);
      strip.setBrightness(brightness);
      for (uint8_t i = 0; i < LED_COUNT; i++)
        strip.setPixelColor(i, r, g, b);
    }
    else
    {
      uint8_t r = map(potValues[3], 0, 1023, 0, 255);
      uint8_t g = map(potValues[4], 0, 1023, 0, 255);
      uint8_t b = map(potValues[5], 0, 1023, 0, 255);
      uint8_t brightness = map(potValues[7], 0, 1023, 0, 255);
      strip.setBrightness(brightness);
      for (uint8_t i = 0; i < LED_COUNT; i++)
        strip.setPixelColor(i, r, g, b);
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz refresh
  }
}
void buttonTask(void *pvParameters)
{
  while (1)
  {
    if (!digitalRead(SELECT_BUTTON))
    {
      if (currentMenu.load() == MENU_MAIN)
        currentMenu.store(PRESET_SELECT);
      else if (currentMenu.load() == PRESET_SELECT)
        currentMenu.store(RGBB_SCREEN);
      else if (currentMenu.load() == RGBB_SCREEN)
        currentMenu.store(NUMBER_SCREEN);
      else if (currentMenu.load() == NUMBER_SCREEN)
      {
        // logic for saving data using counter
        saveColor[selectedMainIndex][selectedPresetIndex][selectedRGBBIndex] = counter.load();
        counter.store(0);
        printSaveColors();
        saveToNVS();
        currentMenu.store(RGBB_SCREEN);
      }
      while (!digitalRead(SELECT_BUTTON))
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    if (!digitalRead(BACK_BUTTON))
    {
      if (currentMenu.load() == PRESET_SELECT)
        currentMenu.store(MENU_MAIN);
      else if (currentMenu == RGBB_SCREEN)
        currentMenu.store(PRESET_SELECT);
      else if (currentMenu.load() == NUMBER_SCREEN)
      {
        currentMenu.store(RGBB_SCREEN);
      }
      while (!digitalRead(BACK_BUTTON))
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Refresh every 100ms
  }
}
void setup()
{
  strip.begin();
  strip.show();
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("OLED init failed"));
    while (true)
      ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();
  loadFromNVS();

  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(PIEZO, INPUT);
  pinMode(SELECT_BUTTON, INPUT_PULLUP);
  pinMode(BACK_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT), updateEncoder, CHANGE);

  xTaskCreatePinnedToCore(
      displayTask,        // Task function
      "Display Task",     // Name
      2048,               // Stack size
      NULL,               // Params
      1,                  // Priority
      &displayTaskHandle, // Handle
      0);                 // Core 0

  xTaskCreatePinnedToCore(
      ledTask,
      "LED Task",
      2048,
      NULL,
      1,
      &ledTaskHandle,
      1); // Core 1
  xTaskCreatePinnedToCore(
      buttonTask,
      "button Task",
      2048,
      NULL,
      1,
      &buttonHandle,
      0); // Core 1
}
void printSaveColors()
{
  for (int menu = 0; menu < menuItemCount; menu++)
  {
    Serial.print("MenuItem ");
    Serial.print(menu);
    Serial.println(":");

    for (int preset = 0; preset < presetMenuItemCount; preset++)
    {
      Serial.print("  Preset ");
      Serial.print(preset);
      Serial.print(" → R: ");
      Serial.print(saveColor[menu][preset][0].load());
      Serial.print(", G: ");
      Serial.print(saveColor[menu][preset][1].load());
      Serial.print(", B: ");
      Serial.print(saveColor[menu][preset][2].load());
      Serial.print(", Brightness: ");
      Serial.println(saveColor[menu][preset][3].load());
    }
  }
}

void menu_screen()
{
  display.clearDisplay();

  for (int i = 0; i < menuItemCount; i++)
  {
    int y = i * 10;
    if (i == selectedMainIndex.load())
    {
      display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }
    else
    {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(2, y);
    display.print(mainMenuItems[i]);
  }
  display.display();
}
void preset_screen()
{
  display.clearDisplay();

  for (int i = 0; i < presetMenuItemCount; i++)
  {
    int y = i * 10;
    if (i == selectedPresetIndex.load())
    {
      display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }
    else
    {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(2, y);
    display.print(presetMenuItems[i]);
  }
  display.display();
}
void rgbb_screen()
{
  display.clearDisplay();

  for (int i = 0; i < RGBBMenuItemCount; i++)
  {
    int y = i * 10;
    if (i == selectedRGBBIndex.load())
    {
      display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }
    else
    {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(2, y);
    display.print(RGBBMenuItems[i]);
  }
  display.display();
}
void number_screen()
{
  display.clearDisplay();
  // Serial.println("ll");
  display.setCursor(2, 0);
  display.print(counter.load());

  display.display();
}
void loop()
{
}
