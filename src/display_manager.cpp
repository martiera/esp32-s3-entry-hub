#include "display_manager.h"
#include "config.h"
#include "pins.h"

DisplayManager display;

DisplayManager::DisplayManager() 
    : initialized(false),
      displayEnabled(false),
      currentBrightness(255),
      lastActivityTime(0),
      autoSleepEnabled(false),
      sleepTimeout(60000) {
}

bool DisplayManager::begin() {
    Serial.println("Initializing display...");
    
    // Initialize TFT
    tft.init();
    tft.setRotation(1);  // Landscape mode (adjust 0-3 as needed)
    
    // IPS display configuration - invert colors for correct display
    tft.invertDisplay(true);  // Enable color inversion for IPS panels
    
    // Set backlight (GPIO 46)
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, currentBrightness);
    
    // Clear screen
    tft.fillScreen(TFT_BLACK);
    
    initialized = true;
    displayEnabled = true;
    
    // Show boot screen
    drawBootScreen();
    
    Serial.println("✓ Display initialized (ILI9488 480x320)");
    return true;
}

void DisplayManager::drawBootScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Title
    tft.setTextSize(3);
    tft.setCursor(80, 60);
    tft.println("ESP32-S3");
    
    tft.setCursor(80, 100);
    tft.println("Entry Hub");
    
    // Version
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(150, 150);
    tft.println("v" DEVICE_VERSION);
    
    // Status
    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(160, 200);
    tft.println("Initializing...");
    
    delay(2000);  // Show boot screen briefly
}

void DisplayManager::loop() {
    if (!initialized || !displayEnabled) {
        return;
    }
    
    // Handle auto-sleep
    if (autoSleepEnabled) {
        unsigned long now = millis();
        if (now - lastActivityTime > sleepTimeout) {
            sleepDisplay();
        }
    }
}

void DisplayManager::showDashboard() {
    if (!initialized) return;
    
    drawStatusScreen();
}

void DisplayManager::drawStatusScreen() {
    tft.fillScreen(TFT_BLACK);
    
    // Header bar
    tft.fillRect(0, 0, 480, 40, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextSize(2);
    tft.setCursor(10, 12);
    tft.println("ESP32-S3 Entry Hub");
    
    // System info section
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 60);
    tft.println("System Status");
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 90);
    tft.print("IP: ");
    tft.println("Loading...");
    
    tft.setCursor(10, 110);
    tft.print("WiFi: ");
    tft.println("Connecting...");
    
    tft.setCursor(10, 130);
    tft.print("MQTT: ");
    tft.println("Checking...");
    
    // Instructions
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 180);
    tft.println("Display initialized successfully!");
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 200);
    tft.println("Access web panel:");
    tft.setCursor(10, 215);
    tft.println("http://entryhub.local");
}

void DisplayManager::showVoiceRecognition() {
    if (!initialized) return;
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(120, 120);
    tft.println("LISTENING");
    
    // Draw animated microphone icon
    tft.fillCircle(240, 200, 30, TFT_GREEN);
}

void DisplayManager::showPresence() {
    if (!initialized) return;
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Presence Status");
}

void DisplayManager::showWeather() {
    if (!initialized) return;
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Weather");
}

void DisplayManager::showCalendar() {
    if (!initialized) return;
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Calendar");
}

void DisplayManager::showNotification(const char* title, const char* message) {
    if (!initialized) return;
    
    // Draw notification popup
    tft.fillRect(40, 100, 400, 120, TFT_DARKGREY);
    tft.drawRect(40, 100, 400, 120, TFT_WHITE);
    
    tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    tft.setTextSize(2);
    tft.setCursor(50, 110);
    tft.println(title);
    
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(50, 140);
    tft.println(message);
}

void DisplayManager::showWakeWordDetected() {
    if (!initialized) return;
    
    // Flash green border
    tft.drawRect(0, 0, 480, 320, TFT_GREEN);
    tft.drawRect(1, 1, 478, 318, TFT_GREEN);
    tft.drawRect(2, 2, 476, 316, TFT_GREEN);
    
    delay(200);
    
    showVoiceRecognition();
}

void DisplayManager::updateWiFiStatus(bool connected, int rssi) {
    if (!initialized) return;
    
    // Update WiFi status on screen
    tft.setTextColor(connected ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.print("WiFi: ");
    tft.print(connected ? "Connected " : "Disconnected");
    tft.print("     ");  // Clear old text
    
    if (connected) {
        tft.setCursor(10, 125);
        tft.print("Signal: ");
        tft.print(rssi);
        tft.println(" dBm      ");
    }
}

void DisplayManager::updateTimeDisplay(const char* time) {
    if (!initialized) return;
    
    // Display time in top-right corner
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextSize(1);
    tft.setCursor(380, 12);
    tft.println(time);
}

void DisplayManager::updateWeather(int temp, const char* condition) {
    if (!initialized) return;
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 150);
    tft.print("Weather: ");
    tft.print(temp);
    tft.print("C ");
    tft.print(condition);
    tft.println("          ");
}

void DisplayManager::setBrightness(uint8_t brightness) {
    currentBrightness = brightness;
    if (initialized) {
        analogWrite(TFT_BL, brightness);
    }
}

void DisplayManager::setAutoSleep(bool enabled, uint32_t timeout) {
    autoSleepEnabled = enabled;
    sleepTimeout = timeout;
    lastActivityTime = millis();
}

void DisplayManager::wakeDisplay() {
    if (!initialized) return;
    
    analogWrite(TFT_BL, currentBrightness);
    displayEnabled = true;
    lastActivityTime = millis();
    showDashboard();
}

void DisplayManager::sleepDisplay() {
    if (!initialized) return;
    
    analogWrite(TFT_BL, 0);
    displayEnabled = false;
}
