#include "display_manager.h"
#include "config.h"
#include "pins.h"
#include <Wire.h>

DisplayManager display;

// Static touch handler for FT6X36 callback
static int16_t lastTouchX = -1;
static int16_t lastTouchY = -1;
static bool touchDetected = false;

void touchHandler(TPoint point, TEvent e) {
    if (e == TEvent::TouchStart || e == TEvent::TouchMove || e == TEvent::Tap) {
        lastTouchX = point.x;
        lastTouchY = point.y;
        touchDetected = true;
    } else if (e == TEvent::TouchEnd) {
        touchDetected = false;
    }
}

DisplayManager::DisplayManager() 
    : initialized(false),
      displayEnabled(false),
      currentBrightness(255),
      lastActivityTime(0),
      autoSleepEnabled(false),
      sleepTimeout(60000),
      touch(&Wire, TOUCH_INT),
      touchInitialized(false),
      touchCallback(nullptr),
      lastTouchEvent(TouchEvent::NONE),
      touchActive(false),
      touchStartX(0), touchStartY(0),
      touchEndX(0), touchEndY(0),
      touchStartTime(0),
      currentTemp(0),
      currentHumidity(0),
      weatherCondition("--"),
      weatherIcon(""),
      lastWeatherUpdate(0),
      voiceButtonActive(false) {
}

bool DisplayManager::begin() {
    log_i("Initializing display...");
    
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
    
    // Initialize I2C for touch
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    
    // Configure interrupt pin
    pinMode(TOUCH_INT, INPUT);
    
    // Initialize touch controller
    log_i("Initializing touch controller on I2C SDA=%d SCL=%d INT=%d", TOUCH_SDA, TOUCH_SCL, TOUCH_INT);
    if (touch.begin(FT6X36_DEFAULT_THRESHOLD)) {
        touchInitialized = true;
        touch.registerTouchHandler(touchHandler);
        log_i("FT6X36 touch controller initialized successfully");
    } else {
        log_e("Touch controller NOT FOUND!");
    }
    
    // Show boot screen
    drawBootScreen();
    
    log_i("Display initialized (ILI9488 480x320)");
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
    
    // Process touch events (INT pin required for callback)
    if (touchInitialized) {
        touch.loop();
        handleTouch();
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
    
    // Draw enhanced UI widgets
    drawWeatherWidget();
    drawTimeWidget();
    drawVoiceButton(false);
    
    // System status section (below widgets)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 160);
    tft.println("System Status:");
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 175);
    tft.print("WiFi: Connected");
    
    tft.setCursor(10, 190);
    tft.print("MQTT: Ready");
    
    // Instructions at bottom
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(10, 280);
    tft.println("Touch voice button or say wake word");
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

// Touch methods
void DisplayManager::handleTouch() {
    if (!touchInitialized) return;
    
    if (touchDetected && lastTouchX >= 0 && lastTouchY >= 0) {
        // Get raw touch coordinates from callback
        int16_t rawX = lastTouchX;
        int16_t rawY = lastTouchY;
        
        // FT6236 touch panel is 320x480 (portrait)
        // Display is rotated to landscape 480x320
        // Transform: rawY -> screenX, inverted rawX -> screenY
        int16_t x = map(rawY, 0, 480, 0, 479);
        int16_t y = map(320 - rawX, 0, 320, 0, 319);
        
        // Clamp to screen bounds
        x = constrain(x, 0, 479);
        y = constrain(y, 0, 319);
        
        if (!touchActive) {
            // Touch just started
            touchActive = true;
            touchStartX = x;
            touchStartY = y;
            touchStartTime = millis();
            log_i("Touch START at (%d,%d)", x, y);
            
            // Check if voice button was pressed
            if (isVoiceButtonPressed(x, y)) {
                log_i("Voice button pressed!");
                triggerVoiceCommand();
                
                // Invoke touch callback if set
                if (touchCallback) {
                    touchCallback(TouchEvent::TAP, x, y);
                }
            }
        }
        
        touchEndX = x;
        touchEndY = y;
        
        // Update activity time
        lastActivityTime = millis();
        
        // Visual feedback - draw touch point
        tft.fillCircle(x, y, 5, TFT_RED);
        
    } else if (touchActive && !touchDetected) {
        // Touch just ended - process gesture
        touchActive = false;
        processTouchGesture();
    }
}

void DisplayManager::processTouchGesture() {
    unsigned long touchDuration = millis() - touchStartTime;
    int16_t deltaX = touchEndX - touchStartX;
    int16_t deltaY = touchEndY - touchStartY;
    
    const int16_t SWIPE_THRESHOLD = 50;
    const unsigned long LONG_PRESS_THRESHOLD = 500;
    
    // Determine gesture type
    if (abs(deltaX) > SWIPE_THRESHOLD || abs(deltaY) > SWIPE_THRESHOLD) {
        // Swipe detected
        if (abs(deltaX) > abs(deltaY)) {
            // Horizontal swipe
            if (deltaX > 0) {
                lastTouchEvent = TouchEvent::SWIPE_RIGHT;
                Serial.printf("Touch: SWIPE RIGHT at (%d, %d)\n", touchEndX, touchEndY);
            } else {
                lastTouchEvent = TouchEvent::SWIPE_LEFT;
                Serial.printf("Touch: SWIPE LEFT at (%d, %d)\n", touchEndX, touchEndY);
            }
        } else {
            // Vertical swipe
            if (deltaY > 0) {
                lastTouchEvent = TouchEvent::SWIPE_DOWN;
                Serial.printf("Touch: SWIPE DOWN at (%d, %d)\n", touchEndX, touchEndY);
            } else {
                lastTouchEvent = TouchEvent::SWIPE_UP;
                Serial.printf("Touch: SWIPE UP at (%d, %d)\n", touchEndX, touchEndY);
            }
        }
    } else if (touchDuration > LONG_PRESS_THRESHOLD) {
        lastTouchEvent = TouchEvent::LONG_PRESS;
        Serial.printf("Touch: LONG PRESS at (%d, %d) for %lu ms\n", touchEndX, touchEndY, touchDuration);
    } else {
        lastTouchEvent = TouchEvent::TAP;
        Serial.printf("Touch: TAP at (%d, %d)\n", touchEndX, touchEndY);
    }
    
    // Call callback if set
    if (touchCallback) {
        touchCallback(lastTouchEvent, touchEndX, touchEndY);
    }
}

bool DisplayManager::isTouched() {
    if (!touchInitialized) return false;
    return touchDetected;
}

void DisplayManager::getTouchPoint(int16_t &x, int16_t &y) {
    if (!touchInitialized || !touchDetected) {
        x = -1;
        y = -1;
        return;
    }
    
    x = lastTouchX;
    y = lastTouchY;
    
    // Transform for landscape
    int16_t tempX = x;
    x = y;
    y = 320 - tempX;
    x = map(x, 0, 319, 0, 479);
}

void DisplayManager::setTouchCallback(TouchCallback callback) {
    touchCallback = callback;
}

TouchEvent DisplayManager::getLastTouchEvent() {
    return lastTouchEvent;
}

// Weather widget drawing
void DisplayManager::drawWeatherWidget() {
    // Weather widget positioned in upper-left area
    int wx = 10, wy = 50, ww = 150, wh = 100;
    
    // Draw rounded background
    tft.fillRoundRect(wx, wy, ww, wh, 10, TFT_DARKGREY);
    tft.drawRoundRect(wx, wy, ww, wh, 10, TFT_LIGHTGREY);
    
    // Display temperature
    tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    tft.setTextSize(3);
    tft.setCursor(wx + 15, wy + 15);
    tft.printf("%.1f", currentTemp);
    tft.setTextSize(2);
    tft.print("C");
    
    // Display humidity
    tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
    tft.setTextSize(2);
    tft.setCursor(wx + 15, wy + 50);
    tft.printf("%d%%", currentHumidity);
    
    // Display weather condition
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(wx + 10, wy + 75);
    tft.print(weatherCondition.substring(0, 14).c_str()); // Truncate to fit
}

// Voice button drawing
void DisplayManager::drawVoiceButton(bool pressed) {
    uint16_t bgColor = pressed ? TFT_GREEN : TFT_BLUE;
    uint16_t borderColor = pressed ? TFT_WHITE : TFT_LIGHTGREY;
    
    // Draw rounded button
    tft.fillRoundRect(VOICE_BTN_X, VOICE_BTN_Y, VOICE_BTN_W, VOICE_BTN_H, 15, bgColor);
    tft.drawRoundRect(VOICE_BTN_X, VOICE_BTN_Y, VOICE_BTN_W, VOICE_BTN_H, 15, borderColor);
    
    // Draw microphone icon (simple representation)
    int cx = VOICE_BTN_X + VOICE_BTN_W/2;
    int cy = VOICE_BTN_Y + VOICE_BTN_H/2 - 10;
    
    // Mic body
    tft.fillRoundRect(cx - 15, cy - 20, 30, 40, 8, TFT_WHITE);
    // Mic stand
    tft.fillRect(cx - 2, cy + 20, 4, 15, TFT_WHITE);
    // Mic base
    tft.fillRect(cx - 10, cy + 35, 20, 3, TFT_WHITE);
    
    // Label
    tft.setTextColor(TFT_WHITE, bgColor);
    tft.setTextSize(2);
    tft.setCursor(VOICE_BTN_X + 20, VOICE_BTN_Y + VOICE_BTN_H - 25);
    tft.print("VOICE");
}

// Time widget drawing
void DisplayManager::drawTimeWidget() {
    // Time widget in upper-center area
    int tx = 170, ty = 50, tw = 140, th = 50;
    
    // Draw background
    tft.fillRoundRect(tx, ty, tw, th, 10, TFT_DARKGREY);
    tft.drawRoundRect(tx, ty, tw, th, 10, TFT_LIGHTGREY);
    
    // Display time (placeholder - needs RTC or NTP)
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(3);
    tft.setCursor(tx + 10, ty + 15);
    tft.print("--:--");
}

// Update weather data
void DisplayManager::updateWeather(float temp, int humidity, const char* condition, const char* icon) {
    currentTemp = temp;
    currentHumidity = humidity;
    weatherCondition = String(condition);
    weatherIcon = String(icon);
    lastWeatherUpdate = millis();
    
    drawWeatherWidget();
}

// Check if voice button is pressed
bool DisplayManager::isVoiceButtonPressed(int16_t x, int16_t y) {
    return (x >= VOICE_BTN_X && x <= VOICE_BTN_X + VOICE_BTN_W &&
            y >= VOICE_BTN_Y && y <= VOICE_BTN_Y + VOICE_BTN_H);
}

// Trigger voice command manually
void DisplayManager::triggerVoiceCommand() {
    voiceButtonActive = true;
    drawVoiceButton(true);
    delay(100); // Visual feedback
    voiceButtonActive = false;
    drawVoiceButton(false);
}
