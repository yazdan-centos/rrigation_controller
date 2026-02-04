#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <Preferences.h>

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

namespace Config {
    // Hardware Pins
    namespace Pins {
        constexpr uint8_t TFT_CS   = 5;
        constexpr uint8_t TFT_DC   = 2;
        constexpr uint8_t TFT_RST  = 4;
        
        constexpr uint8_t BTN_UP    = 32;
        constexpr uint8_t BTN_DOWN  = 33;
        constexpr uint8_t BTN_ENTER = 25;
        constexpr uint8_t BTN_BACK  = 26;
        
        constexpr uint8_t RELAY1 = 27;
        constexpr uint8_t RELAY2 = 14;
    }
    
    // Timing Constants
    namespace Timing {
        constexpr uint32_t DEBOUNCE_MS       = 180;
        constexpr uint32_t SCREEN_UPDATE_MS  = 1000;
        constexpr uint32_t SCHEDULE_CHECK_MS = 1000;
        constexpr uint32_t WIFI_TIMEOUT_MS   = 10000;
        constexpr uint32_t LOOP_DELAY_MS     = 30;
    }
    
    // Display Dimensions (ILI9341 landscape)
    namespace Display {
        constexpr uint16_t WIDTH  = 320;
        constexpr uint16_t HEIGHT = 240;
        constexpr uint8_t  MARGIN = 12;
        constexpr uint8_t  RADIUS = 8;
    }
    
    // Limits
    namespace Limits {
        constexpr uint16_t MAX_DURATION_MIN = 999;
        constexpr uint16_t MIN_DURATION_MIN = 1;
    }
    
    // WiFi Credentials
    constexpr const char* WIFI_SSID = "MobinNet3547";
    constexpr const char* WIFI_PASS = "EFFE3547";
}

// ═══════════════════════════════════════════════════════════════════════════
// COLOR PALETTE - Modern Vibrant Theme
// ═══════════════════════════════════════════════════════════════════════════

namespace Colors {
    // Background colors
    constexpr uint16_t BG_DARK      = 0x1082;  // Deep navy blue
    constexpr uint16_t BG_CARD      = 0x2945;  // Slightly lighter navy
    constexpr uint16_t BG_HEADER    = 0x0841;  // Darker header
    
    // Primary accent colors
    constexpr uint16_t ACCENT_CYAN    = 0x07FF;  // Bright cyan
    constexpr uint16_t ACCENT_MAGENTA = 0xF81F;  // Vibrant magenta
    constexpr uint16_t ACCENT_ORANGE  = 0xFD20;  // Warm orange
    constexpr uint16_t ACCENT_GREEN   = 0x07E0;  // Bright green
    constexpr uint16_t ACCENT_YELLOW  = 0xFFE0;  // Golden yellow
    constexpr uint16_t ACCENT_PURPLE  = 0x881F;  // Rich purple
    
    // Status colors
    constexpr uint16_t STATUS_ON      = 0x07E0;  // Green
    constexpr uint16_t STATUS_OFF     = 0xF800;  // Red
    constexpr uint16_t STATUS_WARNING = 0xFD20;  // Orange
    
    // Text colors
    constexpr uint16_t TEXT_PRIMARY   = 0xFFFF;  // White
    constexpr uint16_t TEXT_SECONDARY = 0xB5B6;  // Light gray
    constexpr uint16_t TEXT_MUTED     = 0x7BEF;  // Muted gray
    
    // Menu colors
    constexpr uint16_t MENU_SELECTED     = 0x2A69;  // Highlighted background
    constexpr uint16_t MENU_SELECTED_TXT = ACCENT_CYAN;
}

// ═══════════════════════════════════════════════════════════════════════════
// DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

enum class IrrigationMode : uint8_t {
    Daily = 0,
    EveryOther = 1,
    Custom = 2
};

struct Settings {
    bool enabled;
    uint16_t durationMin;
    IrrigationMode mode;
    uint8_t dailyHour;
    uint8_t dailyMin;
    uint32_t customTs;
    uint32_t lastRunTs;
};

struct JalaliDate {
    int year;
    int month;
    int day;
};

enum class Screen : uint8_t {
    Main,
    Menu,
    EditDuration,
    EditMode,
    EditDailyTime,
    EditCustom,
    SyncTime,
    InstantConfirm
};

struct EditCustomState {
    int year, month, day, hour, minute;
    uint8_t field;  // 0=year, 1=month, 2=day, 3=hour, 4=minute
};

struct ButtonState {
    bool up, down, enter, back;
    uint32_t lastPressTime;
};

struct IrrigationState {
    bool active;
    uint32_t startTimeMs;
};

// Cache structure for tracking displayed values
struct MainScreenCache {
    int lastYear, lastMonth, lastDay;
    int lastHour, lastMinute, lastSecond;
    bool lastEnabled;
    bool lastIrrigationActive;
    int lastProgress;
    bool initialized;
};

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL OBJECTS
// ═══════════════════════════════════════════════════════════════════════════

Adafruit_ILI9341 tft(Config::Pins::TFT_CS, Config::Pins::TFT_DC, Config::Pins::TFT_RST);
RTC_DS3231 rtc;
Preferences prefs;

// Application State
Settings settings;
Screen currentScreen = Screen::Main;
int menuIndex = 0;
IrrigationState irrigation = {false, 0};
ButtonState buttons = {false, false, false, false, 0};
MainScreenCache mainCache = {0, 0, 0, 0, 0, 0, false, false, 0, false};

// Edit buffers
uint16_t editDuration;
IrrigationMode editMode;
uint8_t editDailyHour, editDailyMin;
EditCustomState editCustom;

// Menu items
const char* const MENU_ITEMS[] = {
    "Enable/Disable",
    "Mode",
    "Duration",
    "Daily Time",
    "Custom Schedule",
    "Sync Time",
    "Instant Start",
    "Exit"
};
constexpr int MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

// ═══════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// Type-safe min/max to avoid Arduino macro issues
template<typename T>
inline T clampValue(T value, T minVal, T maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

template<typename T>
inline T wrapValue(T value, T minVal, T maxVal) {
    if (value < minVal) return maxVal;
    if (value > maxVal) return minVal;
    return value;
}

// ═══════════════════════════════════════════════════════════════════════════
// UI DRAWING UTILITIES
// ═══════════════════════════════════════════════════════════════════════════

class UI {
public:
    // Draw rounded rectangle with optional border
    static void drawRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h, 
                                 uint16_t fillColor, uint16_t borderColor = 0, 
                                 int16_t radius = Config::Display::RADIUS) {
        tft.fillRoundRect(x, y, w, h, radius, fillColor);
        if (borderColor != 0) {
            tft.drawRoundRect(x, y, w, h, radius, borderColor);
        }
    }
    
    // Draw status indicator dot
    static void drawStatusDot(int16_t x, int16_t y, bool active) {
        uint16_t color = active ? Colors::STATUS_ON : Colors::STATUS_OFF;
        tft.fillCircle(x, y, 6, color);
        tft.drawCircle(x, y, 6, Colors::TEXT_PRIMARY);
        // Glow effect
        if (active) {
            tft.drawCircle(x, y, 8, color);
        } else {
            // Clear any previous glow
            tft.drawCircle(x, y, 8, Colors::BG_CARD);
        }
    }
    
    // Draw progress bar
    static void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, 
                                 float progress, uint16_t fillColor) {
        drawRoundedRect(x, y, w, h, Colors::BG_DARK, Colors::TEXT_MUTED, h/2);
        int16_t fillWidth = static_cast<int16_t>(progress * (w - 4));
        if (fillWidth > 0) {
            tft.fillRoundRect(x + 2, y + 2, fillWidth, h - 4, (h-4)/2, fillColor);
        }
    }
    
    // Draw header bar
    static void drawHeader(const char* title, uint16_t accentColor = Colors::ACCENT_CYAN) {
        // Header background with gradient effect
        tft.fillRect(0, 0, Config::Display::WIDTH, 40, Colors::BG_HEADER);
        tft.drawFastHLine(0, 40, Config::Display::WIDTH, accentColor);
        tft.drawFastHLine(0, 41, Config::Display::WIDTH, Colors::BG_DARK);
        
        // Title
        tft.setTextSize(2);
        tft.setTextColor(accentColor);
        int16_t textWidth = strlen(title) * 12;
        tft.setCursor((Config::Display::WIDTH - textWidth) / 2, 12);
        tft.print(title);
    }
    
    // Draw info card
    static void drawInfoCard(int16_t x, int16_t y, int16_t w, int16_t h,
                              const char* label, const char* value, 
                              uint16_t valueColor = Colors::TEXT_PRIMARY) {
        drawRoundedRect(x, y, w, h, Colors::BG_CARD, Colors::TEXT_MUTED);
        
        // Label
        tft.setTextSize(1);
        tft.setTextColor(Colors::TEXT_SECONDARY);
        tft.setCursor(x + 8, y + 6);
        tft.print(label);
        
        // Value
        tft.setTextSize(2);
        tft.setTextColor(valueColor);
        tft.setCursor(x + 8, y + 20);
        tft.print(value);
    }
    
    // Draw icon (simple geometric icons)
    static void drawIcon(int16_t x, int16_t y, uint8_t iconType, uint16_t color) {
        switch (iconType) {
            case 0: // Clock
                tft.drawCircle(x, y, 8, color);
                tft.drawLine(x, y, x, y - 5, color);
                tft.drawLine(x, y, x + 4, y, color);
                break;
            case 1: // Water drop
                tft.fillTriangle(x, y - 8, x - 5, y + 2, x + 5, y + 2, color);
                tft.fillCircle(x, y + 2, 5, color);
                break;
            case 2: // Settings gear
                tft.drawCircle(x, y, 6, color);
                for (int i = 0; i < 8; i++) {
                    float angle = i * PI / 4;
                    int16_t x1 = x + static_cast<int16_t>(cos(angle) * 5);
                    int16_t y1 = y + static_cast<int16_t>(sin(angle) * 5);
                    int16_t x2 = x + static_cast<int16_t>(cos(angle) * 9);
                    int16_t y2 = y + static_cast<int16_t>(sin(angle) * 9);
                    tft.drawLine(x1, y1, x2, y2, color);
                }
                break;
            case 3: // WiFi
                tft.fillCircle(x, y + 4, 2, color);
                for (int r = 6; r <= 12; r += 3) {
                    for (int a = -45; a <= 45; a += 5) {
                        float rad = a * PI / 180.0f;
                        int16_t px = x + static_cast<int16_t>(cos(rad) * r);
                        int16_t py = y + 4 - static_cast<int16_t>(sin(rad) * r);
                        tft.drawPixel(px, py, color);
                    }
                }
                break;
        }
    }
    
    // Draw footer hint
    static void drawFooter(const char* text) {
        tft.fillRect(0, Config::Display::HEIGHT - 25, Config::Display::WIDTH, 25, Colors::BG_HEADER);
        tft.drawFastHLine(0, Config::Display::HEIGHT - 25, Config::Display::WIDTH, Colors::TEXT_MUTED);
        
        tft.setTextSize(1);
        tft.setTextColor(Colors::TEXT_SECONDARY);
        int16_t textWidth = strlen(text) * 6;
        tft.setCursor((Config::Display::WIDTH - textWidth) / 2, Config::Display::HEIGHT - 16);
        tft.print(text);
    }
    
    // Clear a rectangular area with background color
    static void clearArea(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color = Colors::BG_CARD) {
        tft.fillRect(x, y, w, h, color);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// JALALI DATE CONVERSION
// ═══════════════════════════════════════════════════════════════════════════

JalaliDate gregorianToJalali(int gy, int gm, int gd) {
    static const int gDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const int jDaysInMonth[] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29};
    
    int gy2 = (gm > 2) ? (gy + 1) : gy;
    long days = 355666L + (365L * gy) + ((gy2 + 3) / 4) 
                - ((gy2 + 99) / 100) + ((gy2 + 399) / 400) + gd;
    
    for (int i = 0; i < gm - 1; i++) {
        days += gDaysInMonth[i];
    }
    
    long jy = -1595 + (33 * (days / 12053));
    days %= 12053;
    jy += 4 * (days / 1461);
    days %= 1461;
    
    if (days > 365) {
        jy += (days - 1) / 365;
        days = (days - 1) % 365;
    }
    
    int jm = 0;
    for (; jm < 11 && days >= jDaysInMonth[jm]; jm++) {
        days -= jDaysInMonth[jm];
    }
    
    return {static_cast<int>(jy), jm + 1, static_cast<int>(days + 1)};
}

// ═══════════════════════════════════════════════════════════════════════════
// SETTINGS MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

void loadSettings() {
    prefs.begin("irrig", true);
    settings.enabled     = prefs.getBool("enabled", true);
    settings.durationMin = prefs.getUShort("dur", 5);
    settings.mode        = static_cast<IrrigationMode>(prefs.getUChar("mode", 0));
    settings.dailyHour   = prefs.getUChar("dh", 6);
    settings.dailyMin    = prefs.getUChar("dm", 0);
    settings.customTs    = prefs.getULong("cts", 0);
    settings.lastRunTs   = prefs.getULong("last", 0);
    prefs.end();
}

void saveSettings() {
    prefs.begin("irrig", false);
    prefs.putBool("enabled", settings.enabled);
    prefs.putUShort("dur", settings.durationMin);
    prefs.putUChar("mode", static_cast<uint8_t>(settings.mode));
    prefs.putUChar("dh", settings.dailyHour);
    prefs.putUChar("dm", settings.dailyMin);
    prefs.putULong("cts", settings.customTs);
    prefs.putULong("last", settings.lastRunTs);
    prefs.end();
}

// ═══════════════════════════════════════════════════════════════════════════
// SCREEN RENDERING
// ═══════════════════════════════════════════════════════════════════════════

const char* getModeString(IrrigationMode mode) {
    switch (mode) {
        case IrrigationMode::Daily:      return "Daily";
        case IrrigationMode::EveryOther: return "Every 2 Days";
        case IrrigationMode::Custom:     return "Custom";
        default:                          return "Unknown";
    }
}

// Draw only the static elements of the main screen (called once)
void drawMainScreenStatic() {
    tft.fillScreen(Colors::BG_DARK);
    
    // Header
    UI::drawHeader("IRRIGATION SYSTEM", Colors::ACCENT_CYAN);
    
    // Date/Time Card frame (top left)
    UI::drawRoundedRect(10, 50, 145, 55, Colors::BG_CARD, Colors::ACCENT_PURPLE);
    UI::drawIcon(28, 70, 0, Colors::ACCENT_PURPLE);  // Clock icon
    
    tft.setTextSize(1);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.setCursor(45, 55);
    tft.print("DATE & TIME");
    
    // Mode Card (middle left) - static content
    UI::drawRoundedRect(10, 115, 145, 50, Colors::BG_CARD, Colors::ACCENT_ORANGE);
    UI::drawIcon(28, 137, 2, Colors::ACCENT_ORANGE);  // Settings icon
    
    tft.setTextSize(1);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.setCursor(45, 120);
    tft.print("MODE");
    
    tft.setTextSize(2);
    tft.setTextColor(Colors::ACCENT_ORANGE);
    tft.setCursor(45, 135);
    tft.print(getModeString(settings.mode));
    
    // Duration Card (middle right) - static content
    UI::drawRoundedRect(165, 115, 145, 50, Colors::BG_CARD, Colors::ACCENT_MAGENTA);
    UI::drawIcon(183, 137, 1, Colors::ACCENT_MAGENTA);  // Water drop icon
    
    tft.setTextSize(1);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.setCursor(200, 120);
    tft.print("DURATION");
    
    char buffer[32];
    tft.setTextSize(2);
    tft.setTextColor(Colors::ACCENT_MAGENTA);
    tft.setCursor(200, 135);
    snprintf(buffer, sizeof(buffer), "%u min", settings.durationMin);
    tft.print(buffer);
    
    // Footer
    UI::drawFooter("Press ENTER for menu");
    
    // Reset cache to force initial update
    mainCache.initialized = false;
}

// Update only the dynamic elements of the main screen
void updateMainScreenDynamic() {
    DateTime now = rtc.now();
    JalaliDate j = gregorianToJalali(now.year(), now.month(), now.day());
    char buffer[32];
    
    // Check if date changed
    if (!mainCache.initialized || 
        j.year != mainCache.lastYear || 
        j.month != mainCache.lastMonth || 
        j.day != mainCache.lastDay) {
        
        // Clear and redraw date
        UI::clearArea(45, 68, 100, 12, Colors::BG_CARD);
        tft.setTextSize(1);
        tft.setTextColor(Colors::ACCENT_YELLOW);
        tft.setCursor(45, 68);
        snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d", j.year, j.month, j.day);
        tft.print(buffer);
        
        mainCache.lastYear = j.year;
        mainCache.lastMonth = j.month;
        mainCache.lastDay = j.day;
    }
    
    // Check if time changed
    if (!mainCache.initialized ||
        now.hour() != mainCache.lastHour || 
        now.minute() != mainCache.lastMinute || 
        now.second() != mainCache.lastSecond) {
        
        // Clear and redraw time
        UI::clearArea(45, 82, 100, 18, Colors::BG_CARD);
        tft.setTextColor(Colors::TEXT_PRIMARY);
        tft.setCursor(45, 82);
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        tft.setTextSize(2);
        tft.print(buffer);
        
        mainCache.lastHour = now.hour();
        mainCache.lastMinute = now.minute();
        mainCache.lastSecond = now.second();
    }
    
    // Check if enabled status changed
    if (!mainCache.initialized || settings.enabled != mainCache.lastEnabled) {
        // Redraw Status Card (top right) with new border color
        UI::drawRoundedRect(165, 50, 145, 55, Colors::BG_CARD, 
                             settings.enabled ? Colors::STATUS_ON : Colors::STATUS_OFF);
        
        tft.setTextSize(1);
        tft.setTextColor(Colors::TEXT_SECONDARY);
        tft.setCursor(180, 55);
        tft.print("SYSTEM STATUS");
        
        UI::drawStatusDot(295, 77, settings.enabled);
        
        tft.setTextSize(2);
        tft.setTextColor(settings.enabled ? Colors::STATUS_ON : Colors::STATUS_OFF);
        tft.setCursor(180, 72);
        tft.print(settings.enabled ? "ENABLED " : "DISABLED");
        
        mainCache.lastEnabled = settings.enabled;
    }
    
    // Check if irrigation status changed or progress needs update
    int currentProgress = 0;
    if (irrigation.active) {
        uint32_t elapsed = millis() - irrigation.startTimeMs;
        uint32_t total = static_cast<uint32_t>(settings.durationMin) * 60000UL;
        float progress = static_cast<float>(elapsed) / static_cast<float>(total);
        if (progress > 1.0f) progress = 1.0f;
        currentProgress = static_cast<int>(progress * 100);
    }
    
    if (!mainCache.initialized || 
        irrigation.active != mainCache.lastIrrigationActive ||
        (irrigation.active && currentProgress != mainCache.lastProgress)) {
        
        // Redraw Irrigation Status Bar (bottom)
        UI::drawRoundedRect(10, 175, 300, 35, Colors::BG_CARD, 
                             irrigation.active ? Colors::ACCENT_CYAN : Colors::TEXT_MUTED);
        
        tft.setTextSize(1);
        tft.setTextColor(Colors::TEXT_SECONDARY);
        tft.setCursor(20, 180);
        tft.print("IRRIGATION");
        
        if (irrigation.active) {
            float progress = currentProgress / 100.0f;
            
            UI::drawProgressBar(100, 183, 150, 12, progress, Colors::ACCENT_CYAN);
            
            tft.setTextSize(1);
            tft.setTextColor(Colors::ACCENT_CYAN);
            tft.setCursor(260, 183);
            snprintf(buffer, sizeof(buffer), "%d%%", currentProgress);
            tft.print(buffer);
            
            tft.setTextColor(Colors::STATUS_ON);
            tft.setCursor(20, 195);
            tft.print("ACTIVE ");
        } else {
            tft.setTextSize(2);
            tft.setTextColor(Colors::TEXT_MUTED);
            tft.setCursor(100, 185);
            tft.print("STANDBY");
        }
        
        mainCache.lastIrrigationActive = irrigation.active;
        mainCache.lastProgress = currentProgress;
    }
    
    mainCache.initialized = true;
}

// Full main screen draw (for initial draw or when returning to main screen)
void drawMainScreen() {
    drawMainScreenStatic();
    updateMainScreenDynamic();
}

void drawMenu() {
    tft.fillScreen(Colors::BG_DARK);
    UI::drawHeader("SETTINGS MENU", Colors::ACCENT_MAGENTA);
    
    const int startY = 48;
    const int itemHeight = 22;
    const int itemWidth = 296;
    
    for (int i = 0; i < MENU_COUNT; i++) {
        int y = startY + i * itemHeight;
        
        if (i == menuIndex) {
            // Selected item
            UI::drawRoundedRect(12, y, itemWidth, itemHeight - 2, 
                                Colors::MENU_SELECTED, Colors::ACCENT_CYAN, 4);
            tft.setTextColor(Colors::MENU_SELECTED_TXT);
            
            // Selection indicator
            tft.fillTriangle(18, y + 5, 18, y + 15, 26, y + 10, Colors::ACCENT_CYAN);
        } else {
            tft.setTextColor(Colors::TEXT_SECONDARY);
        }
        
        tft.setTextSize(2);
        tft.setCursor(32, y + 3);
        tft.print(MENU_ITEMS[i]);
        
        // Show current value for some items
        if (i == 0) {  // Enable/Disable
            tft.setTextColor(settings.enabled ? Colors::STATUS_ON : Colors::STATUS_OFF);
            tft.setCursor(240, y + 3);
            tft.print(settings.enabled ? "ON" : "OFF");
        }
    }
    
    UI::drawFooter("UP/DOWN: Navigate | ENTER: Select | BACK: Exit");
}

void drawEditDuration() {
    tft.fillScreen(Colors::BG_DARK);
    UI::drawHeader("EDIT DURATION", Colors::ACCENT_MAGENTA);
    
    // Large value display
    UI::drawRoundedRect(60, 80, 200, 70, Colors::BG_CARD, Colors::ACCENT_MAGENTA);
    
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u", editDuration);
    
    tft.setTextSize(4);
    tft.setTextColor(Colors::ACCENT_MAGENTA);
    int textWidth = strlen(buffer) * 24;
    tft.setCursor(160 - textWidth/2 - 20, 100);
    tft.print(buffer);
    
    tft.setTextSize(2);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.setCursor(160 + textWidth/2 - 10, 110);
    tft.print("min");
    
    // Up/Down arrows
    tft.fillTriangle(160, 60, 145, 75, 175, 75, Colors::ACCENT_CYAN);
    tft.fillTriangle(160, 170, 145, 155, 175, 155, Colors::ACCENT_CYAN);
    
    UI::drawFooter("UP/DOWN: Adjust | ENTER: Save | BACK: Cancel");
}

void drawEditMode() {
    tft.fillScreen(Colors::BG_DARK);
    UI::drawHeader("EDIT MODE", Colors::ACCENT_ORANGE);
    
    const char* modes[] = {"Daily", "Every 2 Days", "Custom"};
    const int modeCount = 3;
    
    for (int i = 0; i < modeCount; i++) {
        int y = 70 + i * 45;
        bool selected = (static_cast<int>(editMode) == i);
        
        UI::drawRoundedRect(40, y, 240, 38, 
                            selected ? Colors::MENU_SELECTED : Colors::BG_CARD,
                            selected ? Colors::ACCENT_ORANGE : Colors::TEXT_MUTED);
        
        if (selected) {
            tft.fillCircle(60, y + 19, 8, Colors::ACCENT_ORANGE);
        } else {
            tft.drawCircle(60, y + 19, 8, Colors::TEXT_MUTED);
        }
        
        tft.setTextSize(2);
        tft.setTextColor(selected ? Colors::ACCENT_ORANGE : Colors::TEXT_SECONDARY);
        tft.setCursor(80, y + 10);
        tft.print(modes[i]);
    }
    
    UI::drawFooter("UP/DOWN: Select | ENTER: Confirm | BACK: Cancel");
}

void drawEditDailyTime() {
    tft.fillScreen(Colors::BG_DARK);
    UI::drawHeader("DAILY SCHEDULE", Colors::ACCENT_YELLOW);
    
    // Time display box
    UI::drawRoundedRect(50, 80, 220, 80, Colors::BG_CARD, Colors::ACCENT_YELLOW);
    
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02u : %02u", editDailyHour, editDailyMin);
    
    tft.setTextSize(4);
    tft.setTextColor(Colors::ACCENT_YELLOW);
    tft.setCursor(85, 100);
    tft.print(buffer);
    
    // Labels
    tft.setTextSize(1);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.setCursor(100, 145);
    tft.print("HOUR");
    tft.setCursor(195, 145);
    tft.print("MIN");
    
    // Arrows for hour (UP)
    tft.fillTriangle(115, 65, 100, 78, 130, 78, Colors::ACCENT_CYAN);
    tft.setTextSize(1);
    tft.setTextColor(Colors::ACCENT_CYAN);
    tft.setCursor(105, 52);
    tft.print("UP");
    
    // Arrows for minute (DOWN)
    tft.fillTriangle(210, 175, 195, 162, 225, 162, Colors::ACCENT_MAGENTA);
    tft.setTextColor(Colors::ACCENT_MAGENTA);
    tft.setCursor(195, 180);
    tft.print("DOWN");
    
    UI::drawFooter("UP: Hour+ | DOWN: Min+ | ENTER: Save");
}

void drawEditCustom() {
    tft.fillScreen(Colors::BG_DARK);
    UI::drawHeader("CUSTOM SCHEDULE", Colors::ACCENT_PURPLE);
    
    const char* fieldLabels[] = {"Year", "Month", "Day", "Hour", "Minute"};
    const int fieldValues[] = {editCustom.year, editCustom.month, editCustom.day, 
                               editCustom.hour, editCustom.minute};
    const uint16_t fieldColors[] = {Colors::ACCENT_CYAN, Colors::ACCENT_MAGENTA, 
                                     Colors::ACCENT_ORANGE, Colors::ACCENT_YELLOW, 
                                     Colors::ACCENT_GREEN};
    
    // Date row
    int startX = 20;
    for (int i = 0; i < 3; i++) {
        bool active = (editCustom.field == i);
        int w = (i == 0) ? 90 : 60;
        
        UI::drawRoundedRect(startX, 55, w, 50, 
                            active ? Colors::MENU_SELECTED : Colors::BG_CARD,
                            active ? fieldColors[i] : Colors::TEXT_MUTED, 6);
        
        tft.setTextSize(1);
        tft.setTextColor(Colors::TEXT_SECONDARY);
        tft.setCursor(startX + 8, 60);
        tft.print(fieldLabels[i]);
        
        char buf[8];
        snprintf(buf, sizeof(buf), (i == 0) ? "%04d" : "%02d", fieldValues[i]);
        tft.setTextSize(2);
        tft.setTextColor(active ? fieldColors[i] : Colors::TEXT_PRIMARY);
        tft.setCursor(startX + 8, 78);
        tft.print(buf);
        
        startX += w + 10;
    }
    
    // Time row
    startX = 80;
    for (int i = 3; i < 5; i++) {
        bool active = (editCustom.field == i);
        
        UI::drawRoundedRect(startX, 120, 70, 50, 
                            active ? Colors::MENU_SELECTED : Colors::BG_CARD,
                            active ? fieldColors[i] : Colors::TEXT_MUTED, 6);
        
        tft.setTextSize(1);
        tft.setTextColor(Colors::TEXT_SECONDARY);
        tft.setCursor(startX + 8, 125);
        tft.print(fieldLabels[i]);
        
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d", fieldValues[i]);
        tft.setTextSize(2);
        tft.setTextColor(active ? fieldColors[i] : Colors::TEXT_PRIMARY);
        tft.setCursor(startX + 18, 143);
        tft.print(buf);
        
        startX += 80;
    }
    
    // Separator
    tft.setTextSize(3);
    tft.setTextColor(Colors::TEXT_MUTED);
    tft.setCursor(152, 130);
    tft.print(":");
    
    // Progress indicator
    tft.setTextSize(1);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.setCursor(120, 185);
    char progBuf[16];
    snprintf(progBuf, sizeof(progBuf), "Field %d of 5", editCustom.field + 1);
    tft.print(progBuf);
    
    UI::drawProgressBar(100, 195, 120, 8, (editCustom.field + 1) / 5.0f, Colors::ACCENT_PURPLE);
    
    UI::drawFooter("UP/DOWN: Adjust | ENTER: Next | BACK: Cancel");
}

void drawSyncScreen() {
    tft.fillScreen(Colors::BG_DARK);
    UI::drawHeader("TIME SYNC", Colors::ACCENT_CYAN);
    
    UI::drawRoundedRect(60, 80, 200, 80, Colors::BG_CARD, Colors::ACCENT_CYAN);
    
    UI::drawIcon(160, 110, 3, Colors::ACCENT_CYAN);  // WiFi icon
    
    tft.setTextSize(2);
    tft.setTextColor(Colors::TEXT_PRIMARY);
    tft.setCursor(75, 130);
    tft.print("Connecting...");
}

void drawInstantConfirm() {
    tft.fillScreen(Colors::BG_DARK);
    UI::drawHeader("INSTANT START", Colors::ACCENT_GREEN);
    
    // Warning/confirmation box
    UI::drawRoundedRect(30, 70, 260, 100, Colors::BG_CARD, Colors::ACCENT_GREEN);
    
    UI::drawIcon(160, 95, 1, Colors::ACCENT_GREEN);  // Water drop
    
    tft.setTextSize(2);
    tft.setTextColor(Colors::TEXT_PRIMARY);
    tft.setCursor(55, 115);
    tft.print("Start irrigation?");
    
    // Buttons
    UI::drawRoundedRect(50, 180, 100, 35, Colors::STATUS_ON, Colors::TEXT_PRIMARY);
    tft.setTextSize(2);
    tft.setTextColor(Colors::TEXT_PRIMARY);
    tft.setCursor(70, 188);
    tft.print("YES");
    
    tft.setTextSize(1);
    tft.setCursor(65, 178);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.print("ENTER");
    
    UI::drawRoundedRect(170, 180, 100, 35, Colors::STATUS_OFF, Colors::TEXT_PRIMARY);
    tft.setTextSize(2);
    tft.setTextColor(Colors::TEXT_PRIMARY);
    tft.setCursor(200, 188);
    tft.print("NO");
    
    tft.setTextSize(1);
    tft.setCursor(195, 178);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.print("BACK");
}

// ═══════════════════════════════════════════════════════════════════════════
// INPUT HANDLING
// ═══════════════════════════════════════════════════════════════════════════

bool readButtons() {
    if (millis() - buttons.lastPressTime < Config::Timing::DEBOUNCE_MS) {
        return false;
    }
    
    buttons.up    = (digitalRead(Config::Pins::BTN_UP) == LOW);
    buttons.down  = (digitalRead(Config::Pins::BTN_DOWN) == LOW);
    buttons.enter = (digitalRead(Config::Pins::BTN_ENTER) == LOW);
    buttons.back  = (digitalRead(Config::Pins::BTN_BACK) == LOW);
    
    if (buttons.up || buttons.down || buttons.enter || buttons.back) {
        buttons.lastPressTime = millis();
        return true;
    }
    return false;
}

void handleMainScreen() {
    if (buttons.enter) {
        currentScreen = Screen::Menu;
        menuIndex = 0;
        drawMenu();
    }
}

void handleMenuScreen() {
    if (buttons.up) {
        menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
        drawMenu();
    }
    else if (buttons.down) {
        menuIndex = (menuIndex + 1) % MENU_COUNT;
        drawMenu();
    }
    else if (buttons.back) {
        currentScreen = Screen::Main;
        mainCache.initialized = false;  // Force full redraw
        drawMainScreen();
    }
    else if (buttons.enter) {
        switch (menuIndex) {
            case 0:  // Enable/Disable
                settings.enabled = !settings.enabled;
                saveSettings();
                drawMenu();
                break;
            case 1:  // Mode
                editMode = settings.mode;
                currentScreen = Screen::EditMode;
                drawEditMode();
                break;
            case 2:  // Duration
                editDuration = settings.durationMin;
                currentScreen = Screen::EditDuration;
                drawEditDuration();
                break;
            case 3:  // Daily Time
                editDailyHour = settings.dailyHour;
                editDailyMin = settings.dailyMin;
                currentScreen = Screen::EditDailyTime;
                drawEditDailyTime();
                break;
            case 4:  // Custom Schedule
                {
                    DateTime now = rtc.now();
                    editCustom.year = now.year();
                    editCustom.month = now.month();
                    editCustom.day = now.day();
                    editCustom.hour = now.hour();
                    editCustom.minute = now.minute();
                    editCustom.field = 0;
                    currentScreen = Screen::EditCustom;
                    drawEditCustom();
                }
                break;
            case 5:  // Sync Time
                currentScreen = Screen::SyncTime;
                drawSyncScreen();
                syncTimeWithNTP();
                currentScreen = Screen::Menu;
                drawMenu();
                break;
            case 6:  // Instant Start
                currentScreen = Screen::InstantConfirm;
                drawInstantConfirm();
                break;
            case 7:  // Exit
                currentScreen = Screen::Main;
                mainCache.initialized = false;  // Force full redraw
                drawMainScreen();
                break;
        }
    }
}

void handleEditDuration() {
    bool needsRedraw = false;
    
    if (buttons.up) {
        if (editDuration < Config::Limits::MAX_DURATION_MIN) {
            editDuration++;
        }
        needsRedraw = true;
    }
    else if (buttons.down) {
        if (editDuration > Config::Limits::MIN_DURATION_MIN) {
            editDuration--;
        }
        needsRedraw = true;
    }
    else if (buttons.enter) {
        settings.durationMin = editDuration;
        saveSettings();
        currentScreen = Screen::Menu;
        drawMenu();
        return;
    }
    else if (buttons.back) {
        currentScreen = Screen::Menu;
        drawMenu();
        return;
    }
    
    if (needsRedraw) drawEditDuration();
}

void handleEditMode() {
    bool needsRedraw = false;
    
    if (buttons.up || buttons.down) {
        int mode = static_cast<int>(editMode);
        if (buttons.up) {
            mode = (mode - 1 + 3) % 3;
        } else {
            mode = (mode + 1) % 3;
        }
        editMode = static_cast<IrrigationMode>(mode);
        needsRedraw = true;
    }
    else if (buttons.enter) {
        settings.mode = editMode;
        saveSettings();
        currentScreen = Screen::Menu;
        drawMenu();
        return;
    }
    else if (buttons.back) {
        currentScreen = Screen::Menu;
        drawMenu();
        return;
    }
    
    if (needsRedraw) drawEditMode();
}

void handleEditDailyTime() {
    bool needsRedraw = false;
    
    if (buttons.up) {
        editDailyHour = (editDailyHour + 1) % 24;
        needsRedraw = true;
    }
    else if (buttons.down) {
        editDailyMin = (editDailyMin + 1) % 60;
        needsRedraw = true;
    }
    else if (buttons.enter) {
        settings.dailyHour = editDailyHour;
        settings.dailyMin = editDailyMin;
        saveSettings();
        currentScreen = Screen::Menu;
        drawMenu();
        return;
    }
    else if (buttons.back) {
        currentScreen = Screen::Menu;
        drawMenu();
        return;
    }
    
    if (needsRedraw) drawEditDailyTime();
}

void handleEditCustom() {
    bool needsRedraw = false;
    int delta = buttons.up ? 1 : (buttons.down ? -1 : 0);
    
    if (delta != 0) {
        switch (editCustom.field) {
            case 0: 
                editCustom.year += delta; 
                break;
            case 1: 
                editCustom.month = wrapValue(editCustom.month + delta, 1, 12);
                break;
            case 2:
                editCustom.day = wrapValue(editCustom.day + delta, 1, 31);
                break;
            case 3:
                editCustom.hour = wrapValue(editCustom.hour + delta, 0, 23);
                break;
            case 4:
                editCustom.minute = wrapValue(editCustom.minute + delta, 0, 59);
                break;
        }
        needsRedraw = true;
    }
    else if (buttons.enter) {
        editCustom.field++;
        if (editCustom.field > 4) {
            DateTime dt(editCustom.year, editCustom.month, editCustom.day,
                       editCustom.hour, editCustom.minute, 0);
            settings.customTs = dt.unixtime();
            saveSettings();
            currentScreen = Screen::Menu;
            drawMenu();
            return;
        }
        needsRedraw = true;
    }
    else if (buttons.back) {
        currentScreen = Screen::Menu;
        drawMenu();
        return;
    }
    
    if (needsRedraw) drawEditCustom();
}

void handleInstantConfirm() {
    if (buttons.enter) {
        startIrrigation();
        currentScreen = Screen::Main;
        mainCache.initialized = false;  // Force full redraw
        drawMainScreen();
    }
    else if (buttons.back) {
        currentScreen = Screen::Menu;
        drawMenu();
    }
}

void handleButtons() {
    if (!readButtons()) return;
    
    switch (currentScreen) {
        case Screen::Main:           handleMainScreen(); break;
        case Screen::Menu:           handleMenuScreen(); break;
        case Screen::EditDuration:   handleEditDuration(); break;
        case Screen::EditMode:       handleEditMode(); break;
        case Screen::EditDailyTime:  handleEditDailyTime(); break;
        case Screen::EditCustom:     handleEditCustom(); break;
        case Screen::InstantConfirm: handleInstantConfirm(); break;
        default: break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// IRRIGATION CONTROL
// ═══════════════════════════════════════════════════════════════════════════

void startIrrigation() {
    irrigation.active = true;
    irrigation.startTimeMs = millis();
    digitalWrite(Config::Pins::RELAY1, HIGH);
}

void stopIrrigation() {
    irrigation.active = false;
    digitalWrite(Config::Pins::RELAY1, LOW);
}

void updateIrrigation() {
    if (!irrigation.active) return;
    
    uint32_t elapsed = millis() - irrigation.startTimeMs;
    uint32_t duration = static_cast<uint32_t>(settings.durationMin) * 60000UL;
    
    if (elapsed >= duration) {
        stopIrrigation();
    }
}

void checkSchedule() {
    static uint32_t lastCheck = 0;
    static uint32_t lastScreenUpdate = 0;
    
    uint32_t now = millis();
    
    // Update main screen periodically - only dynamic content
    if (currentScreen == Screen::Main && now - lastScreenUpdate >= Config::Timing::SCREEN_UPDATE_MS) {
        updateMainScreenDynamic();  // Changed from drawMainScreen()
        lastScreenUpdate = now;
    }
    
    // Check schedule
    if (now - lastCheck < Config::Timing::SCHEDULE_CHECK_MS) return;
    lastCheck = now;
    
    if (!settings.enabled || irrigation.active) return;
    
    DateTime rtcNow = rtc.now();
    uint32_t nowTs = rtcNow.unixtime();
    bool shouldRun = false;
    
    switch (settings.mode) {
        case IrrigationMode::Daily:
            if (rtcNow.hour() == settings.dailyHour && 
                rtcNow.minute() == settings.dailyMin &&
                nowTs - settings.lastRunTs > 3600) {
                shouldRun = true;
            }
            break;
            
        case IrrigationMode::EveryOther:
            if (nowTs - settings.lastRunTs >= 48UL * 3600UL) {
                shouldRun = true;
            }
            break;
            
        case IrrigationMode::Custom:
            if (nowTs >= settings.customTs && nowTs - settings.lastRunTs > 3600) {
                shouldRun = true;
            }
            break;
    }
    
    if (shouldRun) {
        startIrrigation();
        settings.lastRunTs = nowTs;
        saveSettings();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// NTP SYNCHRONIZATION
// ═══════════════════════════════════════════════════════════════════════════

void syncTimeWithNTP() {
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASS);
    
    uint32_t startTime = millis();
    int dots = 0;
    
    while (WiFi.status() != WL_CONNECTED && 
           millis() - startTime < Config::Timing::WIFI_TIMEOUT_MS) {
        delay(200);
        
        // Animate dots
        tft.fillRect(75, 130, 180, 20, Colors::BG_CARD);
        tft.setTextSize(2);
        tft.setTextColor(Colors::TEXT_PRIMARY);
        tft.setCursor(75, 130);
        tft.print("Connecting");
        for (int i = 0; i <= dots; i++) {
            tft.print(".");
        }
        dots = (dots + 1) % 4;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        tft.fillRect(75, 130, 180, 20, Colors::BG_CARD);
        tft.setTextColor(Colors::STATUS_ON);
        tft.setCursor(95, 130);
        tft.print("Syncing...");
        
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) {
            rtc.adjust(DateTime(
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec
            ));
            
            tft.fillRect(75, 130, 180, 20, Colors::BG_CARD);
            tft.setTextColor(Colors::STATUS_ON);
            tft.setCursor(85, 130);
            tft.print("Success!");
        } else {
            tft.fillRect(75, 130, 180, 20, Colors::BG_CARD);
            tft.setTextColor(Colors::STATUS_OFF);
            tft.setCursor(75, 130);
            tft.print("Sync Failed");
        }
    } else {
        tft.fillRect(75, 130, 180, 20, Colors::BG_CARD);
        tft.setTextColor(Colors::STATUS_OFF);
        tft.setCursor(65, 130);
        tft.print("No WiFi");
    }
    
    delay(1500);
    WiFi.disconnect(true, true);
}

// ═══════════════════════════════════════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    
    // Initialize buttons
    pinMode(Config::Pins::BTN_UP, INPUT_PULLUP);
    pinMode(Config::Pins::BTN_DOWN, INPUT_PULLUP);
    pinMode(Config::Pins::BTN_ENTER, INPUT_PULLUP);
    pinMode(Config::Pins::BTN_BACK, INPUT_PULLUP);
    
    // Initialize relays
    pinMode(Config::Pins::RELAY1, OUTPUT);
    pinMode(Config::Pins::RELAY2, OUTPUT);
    digitalWrite(Config::Pins::RELAY1, LOW);
    digitalWrite(Config::Pins::RELAY2, LOW);
    
    // Initialize display
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(Colors::BG_DARK);
    
    // Boot splash
    UI::drawHeader("IRRIGATION SYSTEM", Colors::ACCENT_CYAN);
    tft.setTextSize(1);
    tft.setTextColor(Colors::TEXT_SECONDARY);
    tft.setCursor(110, 120);
    tft.print("Initializing...");
    
    // Initialize RTC
    Wire.begin();
    if (!rtc.begin()) {
        tft.setTextColor(Colors::STATUS_OFF);
        tft.setCursor(100, 140);
        tft.print("RTC Error!");
        delay(2000);
    }
    
    // Load settings
    loadSettings();
    
    delay(1000);
    drawMainScreen();
}

void loop() {
    handleButtons();
    checkSchedule();
    updateIrrigation();
    delay(Config::Timing::LOOP_DELAY_MS);
}