#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <FT6X36.h>

// Touch event types
enum class TouchEvent {
    NONE,
    TAP,
    LONG_PRESS,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN
};

// Touch callback function type
typedef void (*TouchCallback)(TouchEvent event, int16_t x, int16_t y);

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    void loop();
    
    // Screen management
    void showDashboard();
    void showVoiceRecognition();
    void showPresence();
    void showWeather();
    void showCalendar();
    
    // Notifications
    void showNotification(const char* title, const char* message);
    void showWakeWordDetected();
    
    // Brightness control
    void setBrightness(uint8_t brightness);
    void setAutoSleep(bool enabled, uint32_t timeout);
    
    // Status updates
    void updateWiFiStatus(bool connected, int rssi);
    void updateTimeDisplay(const char* time);
    void updateWeather(float temp, int humidity, const char* condition, const char* icon);
    
    // Voice button
    void triggerVoiceCommand();
    bool isVoiceButtonPressed(int16_t x, int16_t y);
    
    // Touch
    bool isTouched();
    void getTouchPoint(int16_t &x, int16_t &y);
    void setTouchCallback(TouchCallback callback);
    TouchEvent getLastTouchEvent();
    
private:
    bool initialized;
    bool displayEnabled;
    uint8_t currentBrightness;
    uint32_t lastActivityTime;
    bool autoSleepEnabled;
    uint32_t sleepTimeout;
    
    TFT_eSPI tft;
    FT6X36 touch;
    bool touchInitialized;
    TouchCallback touchCallback;
    TouchEvent lastTouchEvent;
    
    // Touch tracking
    bool touchActive;
    int16_t touchStartX, touchStartY;
    int16_t touchEndX, touchEndY;
    unsigned long touchStartTime;
    
    // Weather data
    float currentTemp;
    int currentHumidity;
    String weatherCondition;
    String weatherIcon;
    unsigned long lastWeatherUpdate;
    
    // Voice button
    static const int VOICE_BTN_X = 350;
    static const int VOICE_BTN_Y = 200;
    static const int VOICE_BTN_W = 110;
    static const int VOICE_BTN_H = 100;
    bool voiceButtonActive;
    
    void initDisplay();
    void drawBootScreen();
    void drawStatusScreen();
    void handleTouch();
    void updateStatusBar();
    void wakeDisplay();
    void sleepDisplay();
    void drawWeatherWidget();
    void drawVoiceButton(bool pressed);
    void drawTimeWidget();
    void processTouchGesture();
};

extern DisplayManager display;

#endif
