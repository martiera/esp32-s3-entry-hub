#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

// Forward declarations for LVGL (to be implemented when display is added)
// #include <lvgl.h>
// #include <TFT_eSPI.h>

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
    void updateWeather(int temp, const char* condition);
    
private:
    bool initialized;
    bool displayEnabled;
    uint8_t currentBrightness;
    uint32_t lastActivityTime;
    bool autoSleepEnabled;
    uint32_t sleepTimeout;
    
    // LVGL objects (to be implemented)
    // lv_obj_t* mainScreen;
    // lv_obj_t* statusBar;
    
    void initializeLVGL();
    void handleTouch();
    void updateStatusBar();
    void wakeDisplay();
    void sleepDisplay();
};

extern DisplayManager display;

#endif
