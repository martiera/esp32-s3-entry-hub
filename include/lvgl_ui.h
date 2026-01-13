#ifndef LVGL_UI_H
#define LVGL_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <FT6X36.h>

// UI Constants
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define MAX_PEOPLE 4
#define MAX_CALENDAR_EVENTS 5

// Screen IDs
enum ScreenID {
    SCREEN_MAIN = 0,
    SCREEN_QUICK_ACTIONS = 1,
    SCREEN_COUNT = 2
};

// Calendar event data
struct CalendarEvent {
    char title[64];
    char time[32];  // e.g., "TODAY 14:00", "TOMORROW 09:00", "All day"
};

// Person presence data
struct PersonData {
    char name[32];
    bool present;
    uint32_t color;
};

class LVGL_UI {
public:
    LVGL_UI();
    
    bool begin();
    void loop();
    
    // Update functions
    void updateTime();
    void updateWeather(float temp, const char* condition);
    void updatePersonPresence(int personIndex, const char* name, bool present, uint32_t color);
    void updateGateStatus(bool isOpen);
    void setAnyoneHome(bool isHome);
    void updateCalendar(CalendarEvent* events, int eventCount);
    
    // Screen navigation
    void showScreen(ScreenID screenId);
    ScreenID getCurrentScreen() { return currentScreen; }
    
    // Voice button callback
    void setVoiceButtonCallback(void (*callback)());
    
    // Voice popup for showing listening/processing status
    void showVoicePopup(const char* statusText, const char* subtitle = nullptr);
    void hideVoicePopup();
    void updateVoicePopupText(const char* statusText, const char* subtitle = nullptr);
    
private:
    TFT_eSPI tft;
    FT6X36 touch;
    
    lv_disp_draw_buf_t draw_buf;
    lv_color_t *buf1;
    lv_color_t *buf2;
    lv_disp_drv_t disp_drv;
    lv_indev_drv_t indev_drv;
    
    // Screens
    lv_obj_t* screens[SCREEN_COUNT];
    ScreenID currentScreen;
    
    // Main screen widgets
    lv_obj_t* bgImage;        // Background image
    
    // Time display
    lv_obj_t* timeLabel;      // Hours
    lv_obj_t* minutesLabel;   // Minutes
    
    // Weather (centered)
    lv_obj_t* weatherContainer;
    lv_obj_t* weatherIcon;    // Weather condition icon (placeholder for now)
    lv_obj_t* tempLabel;
    lv_obj_t* conditionLabel;
    
    // Family presence (2x2 grid in corner)
    lv_obj_t* familyContainer;
    lv_obj_t* personCards[MAX_PEOPLE];
    lv_obj_t* personLabels[MAX_PEOPLE];
    PersonData people[MAX_PEOPLE];
    int personCount;
    
    // Gate status (corner)
    lv_obj_t* gateContainer;
    lv_obj_t* gateIcon;       // Gate icon (placeholder for now)
    lv_obj_t* gateStatusLabel;
    
    // Voice button (small, bottom right)
    lv_obj_t* voiceButton;
    
    // Voice popup overlay
    lv_obj_t* voicePopupOverlay;    // Semi-transparent background
    lv_obj_t* voicePopupContainer;  // Main popup container
    lv_obj_t* voicePopupStatusLabel; // "Listening" / "Processing" text
    lv_obj_t* voicePopupSubtitleLabel; // Recognized words / secondary text
    lv_obj_t* voicePopupAnimation;  // Animated indicator (pulsing circle)
    
    // Calendar card (top right below time)
    lv_obj_t* calendarContainer;
    lv_obj_t* calendarEventLabel;
    lv_obj_t* calendarMoreButton;
    CalendarEvent calendarEvents[MAX_CALENDAR_EVENTS];
    int calendarEventCount;
    
    // Quick Actions screen widgets
    lv_obj_t* quickActionsLabel;
    
    // Callback
    void (*voiceCallback)();
    
    // Screen creation
    void createMainScreen();
    void createQuickActionsScreen();
    
    // Widget creation helpers
    lv_obj_t* createPersonCard(lv_obj_t* parent, const char* name, bool present, lv_coord_t x, lv_coord_t y);
    
    // Static callbacks for LVGL
    static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data);
    static void voice_button_cb(lv_event_t* e);
    static void screen_gesture_cb(lv_event_t* e);
    
    // Singleton instance for static callbacks
    static LVGL_UI* instance;
};

extern LVGL_UI lvglUI;

#endif // LVGL_UI_H
