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

#define JSON_FILE "/settings.json"

#define PIEZO_PIN 15
#define LED_PIN 23
#define LED_COUNT 14
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#define fo4 for (uint8_t i = 0; i < 4; i++)
#define fo6 for (uint8_t i = 0; i < 6; i++)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const uint8_t presetPins[6] = {26, 25, 33, 32, 19, 18};
atomic<bool> presetState[6];
const uint8_t btnPins[4] = {12, 13, 14, 27};
atomic<bool> buttonState[4];

enum btn
{
    up,
    down,
    ok,
    back
};
enum MenuState
{
    MENU_MAIN,
    HITMENU,
    BASEMENU,
    RGB_SCREEN,
    SELECTED_RGB,
    SELECTED_BASE,
    SELECTED_HIT,
    MEM_SCREEN
};
std::atomic<MenuState> currentMenu{MENU_MAIN};
std::atomic<int> selectedMainIndex{0};
constexpr int menuItemCount = 2;
enum class MainMenu
{
    BASE,
    HIT
};
const char *mainMenuItems[menuItemCount] = {
    "Base Color",
    "Hit Color"};

std::atomic<int> selectedHitIndex{0};
constexpr int hitItemCount = 5;
enum class HitMenu
{
    COLOR,
    BRIGHTNESS,
    TAIL,
    CHASE,
    RAINBOW
};
const char *hitItems[hitItemCount] = {
    "Color",
    "Brightness",
    "Tail",
    "Chase",
    "Rainbow",
};

std::atomic<int> selectedBaseIndex{0};
constexpr int baseItemCount = 5;
enum class BaseMenu
{
    COLOR,
    BRIGHTNESS,
    SPEED,
    STROBE,
    RAINBOW
};
const char *baseItems[baseItemCount] = {
    "Color",
    "Brightness",
    "Speed",
    "Strobe",
    "Rainbow",
};

std::atomic<int> selectedRGBIndex{0};
constexpr int RGBItemCount = 3;
const char *RGBItems[RGBItemCount] = {
    "Red",
    "Green",
    "Blue"};

class HitData
{
public:
    atomic<uint8_t> red{255}, blue{255}, green{255}, brightness{100}, tail{3};
    atomic<bool> chase{0}, rainbow{0};
};
class BaseData
{
public:
    atomic<uint8_t> red{255}, blue{0}, green{0}, brightness{100}, speed{2};
    atomic<bool> strobe{0}, rainbow{0};
};
HitData hitData;
BaseData baseData;

atomic<bool> hit(0), base(0);
String mem_screen_data;
void printAllData();
void updateSelectedIndex(std::atomic<int> &index, int itemCount, bool moveUp)
{
    int newIndex = index.load() + (moveUp ? -1 : 1);
    if (newIndex < 0)
        newIndex = 0;
    else if (newIndex >= itemCount)
        newIndex = itemCount - 1;
    index.store(newIndex);
}
void savePresetToJson(uint8_t i)
{
    JsonDocument doc;

    if (SPIFFS.exists(JSON_FILE))
    {
        File file = SPIFFS.open(JSON_FILE, FILE_READ);
        if (file)
        {
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            if (error)
            {
                Serial.println("Failed to parse existing JSON");
                doc.clear();
                return;
            }
        }
    }
    if (i < 3)
    {
        JsonArray baseArray = doc["base"].as<JsonArray>();
        JsonObject base_item = baseArray[i];
        base_item["red"] = baseData.red.load();
        base_item["blue"] = baseData.blue.load();
        base_item["green"] = baseData.green.load();
        base_item["brightness"] = baseData.brightness.load();
        base_item["speed"] = baseData.speed.load();
        base_item["strobe"] = baseData.strobe.load();
        base_item["rainbow"] = baseData.rainbow.load();
    }
    if (i > 2)
    {
        uint8_t index = i - 3;
        Serial.println("sss");
        JsonArray hitArray = doc["hit"].as<JsonArray>();
        JsonObject hit_item = hitArray[index];
        hit_item["red"] = hitData.red.load();
        hit_item["blue"] = hitData.blue.load();
        hit_item["green"] = hitData.green.load();
        hit_item["brightness"] = hitData.brightness.load();
        hit_item["tail"] = hitData.tail.load();
        hit_item["chase"] = hitData.chase.load();
        hit_item["rainbow"] = hitData.rainbow.load();
    }

    File file = SPIFFS.open(JSON_FILE, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }

    serializeJsonPretty(doc, file);
    file.close();
    Serial.println("Preset saved.");
    printAllData();
    Serial.println("✅ settings.json contents:");
    serializeJsonPretty(doc, Serial);
}
void loadPresetToJson(uint8_t i)
{
    JsonDocument doc;

    if (SPIFFS.exists(JSON_FILE))
    {
        File file = SPIFFS.open(JSON_FILE, FILE_READ);
        if (file)
        {
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            if (error)
            {
                Serial.println("Failed to parse existing JSON");
                doc.clear();
                return;
            }
        }
    }
    if (i < 3)
    {
        JsonArray baseArray = doc["base"].as<JsonArray>();
        JsonObject base_item = baseArray[i];
        baseData.red.store(base_item["red"]);
        baseData.blue.store(base_item["blue"]);
        baseData.green.store(base_item["green"]);
        baseData.brightness.store(base_item["brightness"]);
        baseData.speed.store(base_item["speed"]);
        baseData.strobe.store(base_item["strobe"]);
        baseData.rainbow.store(base_item["rainbow"]);
    }
    if (i > 2)
    {
        uint8_t index = i - 3;
        JsonArray hitArray = doc["hit"].as<JsonArray>();
        JsonObject hit_item = hitArray[index];
        hitData.red.store(hit_item["red"]);
        hitData.blue.store(hit_item["blue"]);
        hitData.green.store(hit_item["green"]);
        hitData.brightness.store(hit_item["brightness"]);
        hitData.tail.store(hit_item["tail"]);
        hitData.chase.store(hit_item["chase"]);
        hitData.rainbow.store(hit_item["rainbow"]);
    }
    Serial.println("Preset saved.");
    printAllData();
    Serial.println("✅ settings.json contents:");
    serializeJsonPretty(doc, Serial);
}
void menu_screen();
void base_screen(int selectedWidth);
void hit_screen(int selectedWidth);
void rgb_screen(int selectedWidth);
void mem_screen(String line);
void printAllData()
{
    Serial.println("=== HitData ===");
    Serial.print("Red: ");
    Serial.println(hitData.red.load());
    Serial.print("Green: ");
    Serial.println(hitData.green.load());
    Serial.print("Blue: ");
    Serial.println(hitData.blue.load());
    Serial.print("Brightness: ");
    Serial.println(hitData.brightness.load());
    Serial.print("Tail: ");
    Serial.println(hitData.tail.load());
    Serial.print("Chase: ");
    Serial.println(hitData.chase.load() ? "true" : "false");
    Serial.print("Rainbow: ");
    Serial.println(hitData.rainbow.load() ? "true" : "false");

    Serial.println("=== BaseData ===");
    Serial.print("Red: ");
    Serial.println(baseData.red.load());
    Serial.print("Green: ");
    Serial.println(baseData.green.load());
    Serial.print("Blue: ");
    Serial.println(baseData.blue.load());
    Serial.print("Brightness: ");
    Serial.println(baseData.brightness.load());
    Serial.print("Speed: ");
    Serial.println(baseData.speed.load());
    Serial.print("Strobe: ");
    Serial.println(baseData.strobe.load() ? "true" : "false");
    Serial.print("Rainbow: ");
    Serial.println(baseData.rainbow.load() ? "true" : "false");

    Serial.println("================");
}

void adjustRGBValue(int direction)
{
    std::atomic<uint8_t> *colorPtr = nullptr;

    if (base.load())
    {
        if (selectedRGBIndex == 0)
            colorPtr = &baseData.red;
        else if (selectedRGBIndex == 1)
            colorPtr = &baseData.green;
        else if (selectedRGBIndex == 2)
            colorPtr = &baseData.blue;
    }
    else if (hit.load())
    {
        if (selectedRGBIndex == 0)
            colorPtr = &hitData.red;
        else if (selectedRGBIndex == 1)
            colorPtr = &hitData.green;
        else if (selectedRGBIndex == 2)
            colorPtr = &hitData.blue;
    }

    if (colorPtr != nullptr)
    {
        colorPtr->store(colorPtr->load() + direction);
        vTaskDelay(pdMS_TO_TICKS(1000));
        buttonState[up].store(!digitalRead(btnPins[up]));
        buttonState[down].store(!digitalRead(btnPins[down]));
        while ((direction == 1 ? buttonState[up].load() : buttonState[down].load()))
        {
            colorPtr->store(colorPtr->load() + direction);
            vTaskDelay(pdMS_TO_TICKS(200));
            buttonState[up].store(!digitalRead(btnPins[up]));
            buttonState[down].store(!digitalRead(btnPins[down]));
        }
    }
}

void oledTask(void *pvParameters)
{
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    while (true)
    {
        display.clearDisplay();
        if (currentMenu.load() == MENU_MAIN)
        {
            menu_screen();
        }
        else if (currentMenu.load() == BASEMENU)
            base_screen(0);
        else if (currentMenu.load() == HITMENU)
            hit_screen(0);
        else if (currentMenu.load() == RGB_SCREEN)
            rgb_screen(0);
        else if (currentMenu.load() == SELECTED_RGB)
            rgb_screen(100);
        else if (currentMenu.load() == SELECTED_BASE)
            base_screen(100);
        else if (currentMenu.load() == SELECTED_HIT)
            hit_screen(100);
        else if (currentMenu.load() == MEM_SCREEN)
            mem_screen(mem_screen_data);
        display.display();
        vTaskDelay(pdMS_TO_TICKS(1));
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
            if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedMainIndex, menuItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedMainIndex, menuItemCount, 1);
            }
            else if (buttonState[ok].load())
            {
                if (selectedMainIndex == static_cast<int>(MainMenu::BASE))
                    currentMenu.store(BASEMENU);
                else if (selectedMainIndex == static_cast<int>(MainMenu::HIT))
                    currentMenu.store(HITMENU);
            }
            if (buttonState[back].load())
            {
                printAllData();
            }
        }
        else if (currentMenu == BASEMENU)
        {
            if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedBaseIndex, baseItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedBaseIndex, baseItemCount, 1);
            }
            else if (selectedBaseIndex == static_cast<int>(BaseMenu::COLOR) && buttonState[ok])
            {
                base.store(1);
                hit.store(0);
                currentMenu.store(RGB_SCREEN);
            }
            else if (buttonState[ok].load())
            {
                currentMenu.store(SELECTED_BASE);
            }
            else if (buttonState[back].load())
            {
                currentMenu.store(MENU_MAIN);
            }
        }
        else if (currentMenu == HITMENU)
        {
            if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedHitIndex, hitItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedHitIndex, hitItemCount, 1);
            }
            else if (selectedHitIndex == static_cast<int>(HitMenu::COLOR) && buttonState[ok])
            {
                base.store(0);
                hit.store(1);
                currentMenu.store(RGB_SCREEN);
            }
            else if (buttonState[ok].load())
            {
                currentMenu.store(SELECTED_HIT);
            }
            else if (buttonState[back].load())
            {
                currentMenu.store(MENU_MAIN);
            }
        }
        else if (currentMenu == RGB_SCREEN)
        {
            if (buttonState[up].load() || buttonState[down].load())
            {
                if (buttonState[up].load())
                    updateSelectedIndex(selectedRGBIndex, RGBItemCount, 0);
                else if (buttonState[down].load())
                    updateSelectedIndex(selectedRGBIndex, RGBItemCount, 1);
            }
            else if (buttonState[ok].load())
            {
                currentMenu.store(SELECTED_RGB);
            }
            else if (buttonState[back].load())
            {
                if (base.load())
                    currentMenu.store(BASEMENU);
                else if (hit.load())
                    currentMenu.store(HITMENU);
            }
        }
        else if (currentMenu.load() == SELECTED_RGB)
        {
            if (buttonState[up].load())
            {
                adjustRGBValue(+1);
            }

            if (buttonState[down].load())
            {
                adjustRGBValue(-1);
            }

            else if (buttonState[ok].load())
            {
                currentMenu.store(RGB_SCREEN);
            }
            else if (buttonState[back])
            {
                currentMenu.store(RGB_SCREEN);
            }
        }
        else if (currentMenu.load() == SELECTED_BASE)
        {
            if (selectedBaseIndex == static_cast<int>(BaseMenu::BRIGHTNESS) && buttonState[up])
            {
                baseData.brightness.store(baseData.brightness.load() + 1);
            }
            else if (selectedBaseIndex == static_cast<int>(BaseMenu::BRIGHTNESS) && buttonState[down])
            {
                baseData.brightness.store(baseData.brightness.load() - 1);
            }
            if (selectedBaseIndex == static_cast<int>(BaseMenu::SPEED) && buttonState[up])
            {
                uint8_t speed = baseData.speed.load() + 1;
                if (speed > 9)
                    speed = 9;
                else if (speed < 0)
                    speed = 0;
                baseData.speed.store(speed);
            }
            else if (selectedBaseIndex == static_cast<int>(BaseMenu::SPEED) && buttonState[down])
            {
                uint8_t speed = baseData.speed.load() - 1;
                if (speed > 9)
                    speed = 9;
                else if (speed < 0)
                    speed = 0;
                baseData.speed.store(speed);
            }
            if (selectedBaseIndex == static_cast<int>(BaseMenu::STROBE) && buttonState[up])
            {
                baseData.strobe.store(true);
            }
            else if (selectedBaseIndex == static_cast<int>(BaseMenu::STROBE) && buttonState[down])
            {
                baseData.strobe.store(false);
            }
            if (selectedBaseIndex == static_cast<int>(BaseMenu::RAINBOW) && buttonState[up])
            {
                baseData.rainbow.store(true);
            }
            else if (selectedBaseIndex == static_cast<int>(BaseMenu::RAINBOW) && buttonState[down])
            {
                baseData.rainbow.store(false);
            }
            if (buttonState[ok].load())
            {
                currentMenu.store(BASEMENU);
            }
            else if (buttonState[back].load())
            {
                currentMenu.store(BASEMENU);
            }
        }
        else if (currentMenu.load() == SELECTED_HIT)
        {
            if (selectedHitIndex == static_cast<int>(HitMenu::BRIGHTNESS) && buttonState[up])
            {
                hitData.brightness.store(hitData.brightness.load() + 1);
            }
            else if (selectedHitIndex == static_cast<int>(HitMenu::BRIGHTNESS) && buttonState[down])
            {
                hitData.brightness.store(hitData.brightness.load() - 1);
            }
            if (selectedHitIndex == static_cast<int>(HitMenu::TAIL) && buttonState[up])
            {
                hitData.tail.store(hitData.tail.load() + 1);
            }
            else if (selectedHitIndex == static_cast<int>(HitMenu::TAIL) && buttonState[down])
            {
                hitData.tail.store(hitData.tail.load() - 1);
            }
            if (selectedHitIndex == static_cast<int>(HitMenu::CHASE) && buttonState[up])
            {
                hitData.chase.store(1);
            }
            else if (selectedHitIndex == static_cast<int>(HitMenu::CHASE) && buttonState[down])
            {
                hitData.chase.store(0);
            }
            if (selectedHitIndex == static_cast<int>(HitMenu::RAINBOW) && buttonState[up])
            {
                hitData.rainbow.store(1);
            }
            else if (selectedHitIndex == static_cast<int>(HitMenu::RAINBOW) && buttonState[down])
            {
                hitData.rainbow.store(0);
            }
            if (buttonState[ok].load())
            {
                currentMenu.store(HITMENU);
            }
            else if (buttonState[back].load())
            {
                currentMenu.store(HITMENU);
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

void heartbeatEffect(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    const int ledCount = 14;
    const int pulseDelay1 = 100; // ms - first beat
    const int pulseDelay2 = 200; // ms - second beat
    const int fadeDelay = 15;    // ms - delay between fade steps
    const int fadeSteps = 30;

    // Pulse 1
    strip.setBrightness(brightness);
    for (int i = 0; i < ledCount; i++)
    {
        strip.setPixelColor(i, strip.Color(red, green, blue));
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(pulseDelay1));

    // Off between beats
    strip.clear();
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(pulseDelay1 / 2));

    // Pulse 2
    strip.setBrightness(brightness);
    for (int i = 0; i < ledCount; i++)
    {
        strip.setPixelColor(i, strip.Color(red, green, blue));
    }
    strip.show();
    vTaskDelay(pdMS_TO_TICKS(pulseDelay2));

    // Fade out slowly
    for (int step = fadeSteps; step >= 0; step--)
    {
        uint8_t fadeBrightness = (brightness * step) / fadeSteps;
        strip.setBrightness(fadeBrightness);
        strip.show();
        vTaskDelay(pdMS_TO_TICKS(fadeDelay));
    }

    // Wait before next beat
    vTaskDelay(pdMS_TO_TICKS(500));
}
void ledTask(void *pvParameters)
{
    heartbeatEffect(100, 100, 100, 100);

    static uint8_t hue = 0;
    while (true)
    {
        int isHit = 0;

        isHit = analogRead(PIEZO_PIN);
        if (isHit > 4094)
        {
            uint8_t red = hitData.red.load(),
                    green = hitData.green.load(),
                    blue = hitData.blue.load(),
                    brightness = hitData.brightness.load(),
                    tail = hitData.tail.load();
            bool rainbow = hitData.rainbow.load(),
                 chase = hitData.chase.load();

            const int ledCount = strip.numPixels();

            if (rainbow && chase)
            {
                static int pos = 0;
                static uint8_t hueOffset = 0;
                for (int i = 0; i < strip.numPixels(); i++)
                {
                    strip.setPixelColor(i, 0);
                }

                for (int i = 0; i < tail; i++)
                {
                    int index = (pos - i + strip.numPixels()) % strip.numPixels();

                    uint8_t brightnessScale = 255 - i * (205 / max(1, tail - 1));

                    uint16_t pixelHue = (hueOffset + i * 10) % 360;
                    uint32_t color = strip.gamma32(strip.ColorHSV(pixelHue * 182));
                    uint8_t r = (uint8_t)(color >> 16);
                    uint8_t g = (uint8_t)(color >> 8);
                    uint8_t b = (uint8_t)color;
                    r = (r * brightnessScale) / 255;
                    g = (g * brightnessScale) / 255;
                    b = (b * brightnessScale) / 255;

                    strip.setPixelColor(index, r, g, b);
                }

                strip.setBrightness(brightness);
                strip.show();
                pos = (pos + 1) % strip.numPixels();
                hueOffset = (hueOffset + 1) % 360;

                vTaskDelay(pdMS_TO_TICKS(100));
            }
            else if (rainbow && !chase)
            {
            }
            else if (!rainbow && chase)
            {
                static int pos = 0;

                for (int i = 0; i < strip.numPixels(); i++)
                {
                    strip.setPixelColor(i, 0);
                }

                int steps = tail - 1;
                int stepDrop = brightness / max(1, steps);
                for (int i = 0; i < tail; i++)
                {
                    int index = (pos - i + strip.numPixels()) % strip.numPixels();

                    int level = brightness - i * stepDrop;
                    if (level < 0)
                        level = 0;

                    uint8_t scaledRed = (red * level) / 255;
                    uint8_t scaledGreen = (green * level) / 255;
                    uint8_t scaledBlue = (blue * level) / 255;

                    strip.setPixelColor(index, scaledRed, scaledGreen, scaledBlue);
                }

                strip.setBrightness(brightness);
                strip.show();

                pos = (pos + 1) % strip.numPixels();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            else
            {
                for (int i = 0; i < LED_COUNT; i++)
                {
                    strip.setPixelColor(i, strip.Color(red, green, blue));
                }
                strip.setBrightness(brightness);
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        else
        {
            uint8_t red = baseData.red.load(),
                    green = baseData.green.load(),
                    blue = baseData.blue.load(),
                    brightness = baseData.brightness.load(),
                    speed = baseData.speed.load();
            bool rainbow = baseData.rainbow.load();
            bool strobe = baseData.strobe.load();

            if (rainbow && strobe)
            {
                const int ledCount = strip.numPixels();
                uint8_t baseHue = hue;
                for (int i = 0; i < ledCount; i++)
                {
                    uint8_t pixelHue = baseHue + (i * 256 / ledCount);
                    uint32_t color = strip.gamma32(strip.ColorHSV(pixelHue * 256));
                    strip.setPixelColor(i, color);
                }
                strip.setBrightness(brightness);
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(1000 - (speed * 100)));

                for (int i = 0; i < ledCount; i++)
                {
                    strip.setPixelColor(i, 0);
                }
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(1000 - (speed * 100)));

                hue += 8;
                if (hue >= 255)
                    hue = 0;
            }
            else if (!rainbow && strobe)
            {
                for (int i = 0; i < LED_COUNT; i++)
                {
                    strip.setPixelColor(i, strip.Color(red, green, blue));
                }
                strip.setBrightness(brightness);
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(1000 - (speed * 100)));
                for (int i = 0; i < LED_COUNT; i++)
                {
                    strip.setPixelColor(i, 0);
                }
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(1000 - (speed * 100)));
            }
            else if (rainbow && !strobe)
            {
                const int ledCount = strip.numPixels();
                for (int i = 0; i < ledCount; i++)
                {
                    uint8_t pixelHue = hue + (i * 256 / ledCount);
                    uint32_t color = strip.gamma32(strip.ColorHSV(pixelHue * 256));
                    strip.setPixelColor(i, color);
                }
                strip.setBrightness(brightness);
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
                    strip.setPixelColor(i, strip.Color(red, green, blue));
                }
                strip.setBrightness(brightness);
                strip.show();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
}
void presetTask(void *pvParameters)
{

    while (true)
    {
        bool pressed = false;
        fo6
        {
            presetState[i].store(!digitalRead(presetPins[i]));
            if (presetState[i].load())
                pressed = true;
            if (presetState[i].load() && (currentMenu == MENU_MAIN))
            {
                if (i < 3)
                    mem_screen_data = "Loading Base Preset " + String(i + 1);
                else if (i > 2)
                    mem_screen_data = "Loading Hit Preset " + String((i - 3) + 1);

                MenuState temp = currentMenu.load();
                currentMenu.store(MEM_SCREEN);
                loadPresetToJson(i);
                vTaskDelay(pdMS_TO_TICKS(1000));
                currentMenu.store(temp);
            }
            uint64_t m = millis();
            while (pressed)
            {
                if (millis() - m > 2000)
                {
                    if (presetState[i].load())
                    {
                        if (i < 3 && currentMenu == BASEMENU)
                        {
                            mem_screen_data = "Saving Base Preset " + String(i + 1);
                            savePresetToJson(i);
                        }
                        else if (i > 2 && currentMenu == HITMENU)
                        {
                            mem_screen_data = "Saving Hit Preset " + String((i - 3) + 1);
                            savePresetToJson(i);
                        }
                        else
                        {
                            mem_screen_data = "Wrong Button";
                        }
                        MenuState temp = currentMenu.load();
                        currentMenu.store(MEM_SCREEN);
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        currentMenu.store(temp);
                    }
                }
                pressed = false;
                presetState[i].store(!digitalRead(presetPins[i]));
                if (presetState[i].load())
                    pressed = true;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
TaskHandle_t ledTaskHandle = NULL;
void restartLedTask()
{
    if (ledTaskHandle != NULL)
    {
        vTaskDelete(ledTaskHandle);
        ledTaskHandle = NULL;
    }
    xTaskCreate(ledTask, "LED Task", 2048, NULL, 1, &ledTaskHandle);
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
    fo6
    {
        pinMode(presetPins[i], INPUT_PULLUP);
    }
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS mount failed");
    }
    pinMode(PIEZO_PIN, INPUT_PULLUP);
    strip.begin();
    strip.show();
    xTaskCreate(oledTask, "OLED Task", 4096, NULL, 1, NULL);
    xTaskCreate(buttonTask, "Task Task", 4096, NULL, 1, NULL);
    xTaskCreate(ledTask, "LED Task", 2048, NULL, 1, &ledTaskHandle);
    xTaskCreate(presetTask, "Preset Task", 4096, NULL, 1, NULL);
}
void loop()
{
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
void base_screen(int selectedWidth)
{
    display.clearDisplay();

    for (int i = 0; i < baseItemCount; i++)
    {
        int y = i * 10;
        if (i == selectedBaseIndex.load())
        {
            display.fillRect(selectedWidth, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }

        display.setCursor(2, y);
        String line = String(baseItems[i]) + " : ";
        display.print(line);
        display.setCursor(102, y);
        line = "";
        if (i == static_cast<int>(BaseMenu::BRIGHTNESS))
        {
            line += String(baseData.brightness.load());
        }
        else if (i == static_cast<int>(BaseMenu::SPEED))
        {
            line += String(baseData.speed.load());
        }
        else if (i == static_cast<int>(BaseMenu::STROBE))
        {
            line += String(baseData.strobe.load() ? "On" : "Off");
        }
        else if (i == static_cast<int>(BaseMenu::RAINBOW))
        {
            line += String(baseData.rainbow.load() ? "On" : "Off");
        }

        display.print(line);
    }
    display.display();
}
void hit_screen(int selectedWidth)
{
    display.clearDisplay();

    for (int i = 0; i < hitItemCount; i++)
    {
        int y = i * 10;
        if (i == selectedHitIndex.load())
        {
            display.fillRect(selectedWidth, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }

        display.setCursor(2, y);
        String line = String(hitItems[i]) + " : ";
        display.print(line);
        display.setCursor(102, y);
        line = "";

        if (i == static_cast<int>(HitMenu::BRIGHTNESS))
        {
            line += String(hitData.brightness.load());
        }
        else if (i == static_cast<int>(HitMenu::TAIL))
        {
            line += String(hitData.tail.load());
        }
        else if (i == static_cast<int>(HitMenu::CHASE))
        {
            line += String(hitData.chase.load() ? "On" : "Off");
        }
        else if (i == static_cast<int>(HitMenu::RAINBOW))
        {
            line += String(hitData.rainbow.load() ? "On" : "Off");
        }

        display.print(line);
    }
    display.display();
}
void rgb_screen(int selectWidth)
{
    display.clearDisplay();

    for (int i = 0; i < RGBItemCount; i++)
    {
        int y = i * 10;
        if (i == selectedRGBIndex.load())
        {
            display.fillRect(selectWidth, y, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }

        display.setCursor(2, y);
        String line = String(RGBItems[i]) + " : ";
        display.print(line);
        display.setCursor(102, y);
        line = "";
        if (hit.load())
        {
            if (i == 0)
                line += hitData.red.load();
            else if (i == 1)
                line += hitData.green.load();
            else if (i == 2)
                line += hitData.blue.load();
        }
        else if (base.load())
        {
            if (i == 0)
                line += baseData.red.load();
            else if (i == 1)
                line += baseData.green.load();
            else if (i == 2)
                line += baseData.blue.load();
        }
        display.print(line);
    }
    display.display();
}
void mem_screen(String line)
{
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print(line);
    display.display();
}