#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <FS.h>
#include <SPIFFS.h>
#include "json.h"

using namespace std;

#define LED_PIN 23
#define LED_COUNT 14
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define fo4 for (uint8_t i = 0; i < 4; i++)
const uint8_t btnPins[4] = {12, 13, 14, 27};

atomic<bool> buttonState[4];
enum btn
{
    up,
    down,
    ok,
    back
};
atomic<btn> bttn;
enum MenuState
{
    MENU_MAIN,
    SETTINGS,
    RGB_SCREEN,
    NUMBER_SCREEN,
    RAINBOW_SCREEN
};
std::atomic<MenuState> currentMenu{MENU_MAIN};

atomic<uint8_t> counter{0};

std::atomic<int> selectedMainIndex{0};
constexpr int menuItemCount = 2;
const char *mainMenuItems[menuItemCount] = {
    "Base Color",
    "Hit Color"};

std::atomic<int> selectedSettingsIndex{0};
constexpr int SettingsItemCount = 3;
const char *SettingsItems[SettingsItemCount] = {
    "Color",
    "Brightness",
    "Rainbow",
};

std::atomic<int> selectedRGBIndex{0};
constexpr int RGBItemCount = 4;
const char *RGBItems[RGBItemCount] = {
    "Red Channel",
    "Blue Channel",
    "Green Channel", ""};

std::atomic<int> selectedrainbowIndex{0};
constexpr int rainbowItemCount = 2;
const char *rainbowItems[rainbowItemCount] = {
    "OFF",
    "ON"};

void menu_screen();
void settings_screen();
void rgb_screen();
void number_screen();
void rainbow_screen();

class Data
{
public:
    int red, blue, green, brightness, rainbow;
};
Data presets[6];
atomic<int> selectedPreset(0);

const uint8_t presetButtons[6] = {32, 33, 34, 35, 26, 25};

void updateSelectedIndex(std::atomic<int> &index, int itemCount, bool moveUp)
{
    int newIndex = index.load() + (moveUp ? -1 : 1);
    if (newIndex < 0)
        newIndex = 0;
    else if (newIndex >= itemCount)
        newIndex = itemCount - 1;

    index.store(newIndex);
}
void savePresetsToJSON()
{
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    for (int i = 0; i < 6; i++)
    {
        JsonObject obj = array.add<JsonObject>();
        obj["red"] = presets[i].red;
        obj["green"] = presets[i].green;
        obj["blue"] = presets[i].blue;
        obj["brightness"] = presets[i].brightness;
        obj["rainbow"] = presets[i].rainbow;
    }

    File file = SPIFFS.open("/presets.json", "w");
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }

    if (serializeJson(doc, file) == 0)
    {
        Serial.println("Failed to write JSON");
    }
    else
    {
        Serial.println("Presets saved to JSON");
    }

    file.close();
}

void loadPresetsFromJSON()
{
    File file = SPIFFS.open("/presets.json", FILE_READ);
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err)
    {
        Serial.println("Failed to parse JSON");
        return;
    }

    JsonArray array = doc.as<JsonArray>();
    for (int i = 0; i < 6 && i < array.size(); i++)
    {
        JsonObject obj = array[i];
        presets[i].red = obj["red"] | 0;
        presets[i].green = obj["green"] | 0;
        presets[i].blue = obj["blue"] | 0;
        presets[i].brightness = obj["brightness"] | 0;
        presets[i].rainbow = obj["rainbow"] | 0;
    }

    Serial.println("Presets loaded from JSON");
}

void printPresets()
{
    Serial.print("----- Presets -----   ");
    Serial.println(selectedPreset);
    for (int i = 0; i < 6; i++)
    {
        Serial.print("Preset ");
        Serial.print(i);
        Serial.print(": ");

        Serial.print("R=");
        Serial.print(presets[i].red);
        Serial.print(", G=");
        Serial.print(presets[i].green);
        Serial.print(", B=");
        Serial.print(presets[i].blue);
        Serial.print(", Brightness=");
        Serial.print(presets[i].brightness);
        Serial.print(", Rainbow=");
        Serial.println(presets[i].rainbow);
    }
    Serial.println("-------------------");
}

void oledTask(void *pvParameters)
{
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    while (true)
    {
        display.clearDisplay();

        if (currentMenu.load() == MENU_MAIN)
            menu_screen();
        else if (currentMenu.load() == SETTINGS)
            settings_screen();
        else if (currentMenu.load() == RGB_SCREEN)
            rgb_screen();
        else if (currentMenu.load() == NUMBER_SCREEN)
            number_screen();
        else if (currentMenu.load() == RAINBOW_SCREEN)
            rainbow_screen();
        display.display();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
void ledTask(void *pvParameters)
{
    strip.begin();
    strip.show();
    while (true)
    {
        Data preset = presets[selectedPreset.load()];
        if (preset.rainbow)
        {
            static uint8_t hue = 0;
            const int ledCount = strip.numPixels(); // or set manually

            for (int i = 0; i < ledCount; i++)
            {
                uint8_t pixelHue = hue + (i * 256 / ledCount); // evenly spaced hue
                uint32_t color = strip.gamma32(strip.ColorHSV(pixelHue * 256));
                strip.setPixelColor(i, color);
            }

            strip.setBrightness(preset.brightness);
            strip.show();

            hue++;
            if (hue >= 255)
                hue = 0;

            vTaskDelay(pdMS_TO_TICKS(20));
        }
        else
        {
            for (int i = 0; i < LED_COUNT; i++)
            {
                strip.setPixelColor(i, strip.Color(
                                           preset.red,
                                           preset.green,
                                           preset.blue));
            }
            strip.setBrightness(preset.brightness);
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void buttonTask(void *pvParameters)
{
    while (1)
    {
        bool pressed = false;
        fo4
        {
            buttonState[i].store(!digitalRead(btnPins[i]));
            if (buttonState[i].load())
                pressed = true;
        }
        if (currentMenu == MENU_MAIN)
        {
            if (buttonState[ok].load())
            {
                currentMenu.store(SETTINGS);
            }
            else if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedMainIndex, menuItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedMainIndex, menuItemCount, 1);
            }
        }
        else if (currentMenu.load() == SETTINGS)
        {
            if (buttonState[back].load())
                currentMenu.store(MENU_MAIN);
            else if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedSettingsIndex, SettingsItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedSettingsIndex, SettingsItemCount, 1);
            }
            if (buttonState[ok].load() && selectedSettingsIndex.load() == 0)
            {
                currentMenu.store(RGB_SCREEN);
            }
            else if (buttonState[ok].load() && selectedSettingsIndex.load() == 1)
            {
                currentMenu.store(NUMBER_SCREEN);
            }
            else if (buttonState[ok].load() && selectedSettingsIndex.load() == 2)
            {
                currentMenu.store(RAINBOW_SCREEN);
            }
        }
        else if (currentMenu.load() == RGB_SCREEN)
        {
            if (buttonState[back].load())
            {
                currentMenu.store(SETTINGS);
            }
            else if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedRGBIndex, RGBItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedRGBIndex, RGBItemCount, 1);
            }
            if (buttonState[ok].load())
            {
                currentMenu.store(NUMBER_SCREEN);
            }
        }
        else if (currentMenu.load() == NUMBER_SCREEN)
        {
            if (buttonState[ok].load() || buttonState[back].load())
            {
                if (selectedSettingsIndex == 0)
                {
                    if (selectedMainIndex == 0)
                    {
                        if (selectedRGBIndex == 0)
                        {
                            presets[selectedPreset].red = counter.load();
                        }
                        else if (selectedRGBIndex == 1)
                        {
                            presets[selectedPreset].blue = counter.load();
                        }
                        else if (selectedRGBIndex == 2)
                        {
                            presets[selectedPreset].green = counter.load();
                        }
                    }
                    else if (selectedMainIndex == 1)
                    {
                        if (selectedRGBIndex == 0)
                        {
                            presets[selectedPreset + 3].red = counter.load();
                        }
                        else if (selectedRGBIndex == 1)
                        {
                            presets[selectedPreset + 3].blue = counter.load();
                        }
                        else if (selectedRGBIndex == 2)
                        {
                            presets[selectedPreset + 3].green = counter.load();
                        }
                    }
                }
                else if (selectedSettingsIndex == 1)
                {
                    if (selectedMainIndex == 0)
                    {
                        presets[selectedPreset].brightness = counter.load();
                    }
                    else if (selectedMainIndex == 1)
                    {
                        presets[selectedPreset + 3].brightness = counter.load();
                    }
                }
                savePresetsToJSON();
                printPresets();
                currentMenu.store(SETTINGS);
            }
            else if (buttonState[up].load())
            {
                vTaskDelay(pdMS_TO_TICKS(300));
                counter.store(counter.load() + 1);
                buttonState[up].store(!digitalRead(btnPins[up]));

                while (buttonState[up].load())
                {
                    buttonState[up].store(!digitalRead(btnPins[up]));
                    counter.store(counter.load() + 1);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
            else if (buttonState[down].load())
            {
                vTaskDelay(pdMS_TO_TICKS(300));
                counter.store(counter.load() - 1);
                buttonState[down].store(!digitalRead(btnPins[down]));

                while (buttonState[down].load())
                {
                    buttonState[down].store(!digitalRead(btnPins[down]));
                    counter.store(counter.load() - 1);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }
        else if (currentMenu.load() == RAINBOW_SCREEN)
        {
            if (buttonState[ok].load() || buttonState[back].load())
            {
                if (selectedMainIndex == 0)
                {
                    presets[selectedPreset].rainbow = selectedrainbowIndex;
                }
                else if (selectedMainIndex == 1)
                {
                    presets[selectedPreset + 3].rainbow = selectedrainbowIndex;
                }
                savePresetsToJSON();
                printPresets();

                currentMenu.store(SETTINGS);
            }
            else if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedrainbowIndex, rainbowItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedrainbowIndex, rainbowItemCount, 1);
            }
        }
        while (pressed)
        {
            pressed = false;
            fo4
            {
                buttonState[i].store(!digitalRead(btnPins[i]));
                if (buttonState[i].load())
                    pressed = true;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
void presetButtonTask(void *pvParameters)
{

    while (true)
    {
        for (int i = 0; i < 6; i++)
        {
            if (!digitalRead(presetButtons[i]))
            {
                selectedPreset.store(i);
                while (!digitalRead(presetButtons[i]))
                    vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup()
{
    Serial.begin(9600);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("SSD1306 init failed");
        while (true)
            ;
    }

    fo4
    {
        pinMode(btnPins[i], INPUT_PULLUP);
        buttonState[i].store(false);
    }
    for (int i = 0; i < 6; i++)
    {
        pinMode(presetButtons[i], INPUT_PULLUP);
    }

    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS mount failed");
        return;
    }
    loadPresetsFromJSON();
    printPresets();
    // Create tasks
    xTaskCreate(oledTask, "OLED Task", 2048, NULL, 1, NULL);
    xTaskCreate(buttonTask, "Task Task", 4096, NULL, 1, NULL);
    xTaskCreate(ledTask, "LED Task", 2048, NULL, 1, NULL);
    xTaskCreate(presetButtonTask, "PresetBtn", 1024, NULL, 1, NULL);
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
void rainbow_screen()
{
    display.clearDisplay();

    for (int i = 0; i < rainbowItemCount; i++)
    {
        int y = i * 10;
        if (i == selectedrainbowIndex.load())
        {
            display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }

        display.setCursor(2, y);
        display.print(rainbowItems[i]);
    }
    display.display();
}
void settings_screen()
{
    display.clearDisplay();

    for (int i = 0; i < SettingsItemCount; i++)
    {
        int y = i * 10;
        if (i == selectedSettingsIndex.load())
        {
            display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }

        display.setCursor(2, y);
        display.print(SettingsItems[i]);
    }
    display.display();
}
void rgb_screen()
{
    display.clearDisplay();

    for (int i = 0; i < RGBItemCount; i++)
    {
        int y = i * 10;
        if (i == selectedRGBIndex.load())
        {
            display.fillRect(0, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }

        display.setCursor(2, y);
        display.print(RGBItems[i]);
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
    // Optional: Reset buttons after reading or add release logic if needed
}
