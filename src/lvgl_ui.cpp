#include "lvgl_ui.h"
#include "pins.h"
#include <Wire.h>
#include <Ticker.h>

// Custom font with Latin Extended support
#include "montserrat_extended.h"

// External background image
extern const lv_img_dsc_t bg_image;

// Weather icons
#include "weather_icons/weather_icons.h"

LVGL_UI lvglUI;
LVGL_UI* LVGL_UI::instance = nullptr;

// Ticker for LVGL tick
static Ticker lvglTicker;

static void lv_tick_task(void) {
    lv_tick_inc(5);  // 5ms tick
}

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

LVGL_UI::LVGL_UI() 
    : touch(&Wire, TOUCH_INT),
      currentScreen(SCREEN_MAIN),
      personCount(0),
      voiceCallback(nullptr) {
    instance = this;
    
    for (int i = 0; i < SCREEN_COUNT; i++) {
        screens[i] = nullptr;
    }
    
    for (int i = 0; i < MAX_PEOPLE; i++) {
        personCards[i] = nullptr;
        personLabels[i] = nullptr;
        people[i].name[0] = '\0';
        people[i].present = false;
        people[i].color = 0x808080;
    }
}

bool LVGL_UI::begin() {
    log_i("Initializing LVGL UI...");
    
    // Initialize I2C for touch
    Wire.begin(TOUCH_SDA, TOUCH_SCL, I2C_FREQ);
    
    // Initialize TFT
    tft.begin();
    tft.setRotation(1);  // Landscape mode
    
    // IPS display requires inversion ON
    tft.invertDisplay(true);
    
    tft.fillScreen(TFT_BLACK);
    
    // Turn on backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    log_i("Backlight enabled on pin %d", TFT_BL);
    
    // Initialize touch
    if (touch.begin(FT6X36_DEFAULT_THRESHOLD)) {
        touch.registerTouchHandler(touchHandler);
        log_i("FT6X36 touch initialized");
    } else {
        log_e("Failed to initialize FT6X36 touch");
    }
    
    // Initialize LVGL
    lv_init();
    
    // Start LVGL tick timer (5ms)
    lvglTicker.attach_ms(5, lv_tick_task);;
    
    // Allocate LVGL draw buffers (use PSRAM if available)
    // Use full screen buffers for best performance with PSRAM
    size_t buf_size = SCREEN_WIDTH * SCREEN_HEIGHT; 
    buf1 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));
    buf2 = (lv_color_t*)ps_malloc(buf_size * sizeof(lv_color_t));
    
    if (!buf1 || !buf2) {
        log_e("Failed to allocate LVGL buffers");
        return false;
    }
    
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);
    
    // Register display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    // disp_drv.full_refresh = 1; // Optional: Force full refresh if artifacts appear
    lv_disp_drv_register(&disp_drv);
    
    // Register input device (touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // Create screens
    createMainScreen();
    createQuickActionsScreen();
    
    // Load main screen
    lv_scr_load(screens[SCREEN_MAIN]);
    
    log_i("LVGL UI initialized");
    return true;
}

void LVGL_UI::loop() {
    lv_timer_handler();
    delay(5);
    
    // Update time display every second
    static unsigned long lastTimeUpdate = 0;
    if (millis() - lastTimeUpdate > 1000) {
        updateTime();
        lastTimeUpdate = millis();
    }
}

void LVGL_UI::updateTime() {
    if (!timeLabel) return;
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format time as HH:MM
    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    
    lv_label_set_text(timeLabel, timeStr);
}

void LVGL_UI::createMainScreen() {
    screens[SCREEN_MAIN] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_MAIN], lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(screens[SCREEN_MAIN], 0, 0);
    lv_obj_set_style_outline_width(screens[SCREEN_MAIN], 0, 0);
    lv_obj_set_style_pad_all(screens[SCREEN_MAIN], 0, 0);
    
    // Add swipe gesture
    lv_obj_add_event_cb(screens[SCREEN_MAIN], screen_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(screens[SCREEN_MAIN], LV_OBJ_FLAG_SCROLLABLE);
    
    // ========== BACKGROUND IMAGE ==========
    bgImage = lv_img_create(screens[SCREEN_MAIN]);
    lv_img_set_src(bgImage, &bg_image);
    lv_obj_set_pos(bgImage, 0, 0);
    lv_obj_set_style_border_width(bgImage, 0, 0);
    lv_obj_set_style_outline_width(bgImage, 0, 0);
    lv_obj_set_style_pad_all(bgImage, 0, 0);
    lv_obj_set_style_img_recolor_opa(bgImage, LV_OPA_TRANSP, 0);
    
    // ========== GATE (Top Left: 96x80) ==========
    gateContainer = lv_obj_create(screens[SCREEN_MAIN]);
    lv_obj_set_size(gateContainer, 96, 80);
    lv_obj_set_pos(gateContainer, 0, 0);
    lv_obj_set_style_bg_opa(gateContainer, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(gateContainer, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_radius(gateContainer, 12, 0);
    lv_obj_set_style_border_width(gateContainer, 0, 0);
    lv_obj_clear_flag(gateContainer, LV_OBJ_FLAG_SCROLLABLE);
    
    gateIcon = lv_obj_create(gateContainer);
    lv_obj_set_size(gateIcon, 40, 40);
    lv_obj_set_style_radius(gateIcon, 20, 0);
    lv_obj_set_style_bg_color(gateIcon, lv_color_hex(0x505672), 0);
    lv_obj_set_style_border_width(gateIcon, 0, 0);
    lv_obj_align(gateIcon, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(gateIcon, LV_OBJ_FLAG_SCROLLABLE);
    
    gateStatusLabel = lv_label_create(gateContainer);
    lv_label_set_text(gateStatusLabel, "Gate");
    lv_obj_set_style_text_font(gateStatusLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(gateStatusLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(gateStatusLabel, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // ========== TIME (Top Right: 96x40) ==========
    lv_obj_t* timeContainer = lv_obj_create(screens[SCREEN_MAIN]);
    lv_obj_set_size(timeContainer, 96, 40);
    lv_obj_set_pos(timeContainer, 384, 0);
    lv_obj_set_style_bg_opa(timeContainer, LV_OPA_TRANSP, 0);  // Transparent
    lv_obj_set_style_border_width(timeContainer, 0, 0);
    lv_obj_clear_flag(timeContainer, LV_OBJ_FLAG_SCROLLABLE);
    
    timeLabel = lv_label_create(timeContainer);
    lv_label_set_text(timeLabel, "--:--");
    lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(timeLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(timeLabel);
    
    // ========== WEATHER (Large Center: 288x200 - no card, full area) ==========
    weatherContainer = lv_obj_create(screens[SCREEN_MAIN]);
    lv_obj_set_size(weatherContainer, 288, 200);
    lv_obj_set_pos(weatherContainer, 96, 0);
    lv_obj_set_style_bg_opa(weatherContainer, LV_OPA_TRANSP, 0);  // Transparent, no card
    lv_obj_set_style_border_width(weatherContainer, 0, 0);
    lv_obj_set_style_pad_all(weatherContainer, 15, 0);
    lv_obj_clear_flag(weatherContainer, LV_OBJ_FLAG_SCROLLABLE);
    
    // Weather icon (image widget for actual icon) - larger size
    weatherIcon = lv_img_create(weatherContainer);
    lv_img_set_src(weatherIcon, &clear_day);  // Default to sunny
    lv_img_set_zoom(weatherIcon, 384);  // 1.5x scale (256 = 1.0x, 384 = 1.5x)
    lv_obj_set_pos(weatherIcon, 10, 5);
    lv_obj_clear_flag(weatherIcon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(weatherIcon, LV_OBJ_FLAG_HIDDEN);  // Hide until data loaded
    
    // Temperature label (very large, positioned lower with bigger font)
    tempLabel = lv_label_create(weatherContainer);
    lv_label_set_text(tempLabel, "--°");
    lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(tempLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(tempLabel, 90, 90);
    
    // Condition label
    conditionLabel = lv_label_create(weatherContainer);
    lv_label_set_text(conditionLabel, "--");
    lv_obj_set_style_text_font(conditionLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(conditionLabel, lv_color_hex(0xB0B0B0), 0);
    lv_obj_align(conditionLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // ========== USER 1-4 (Bottom rows: 186x48 each with gaps, aligned to bottom) ==========
    for (int i = 0; i < MAX_PEOPLE; i++) {
        int col = i % 2;
        int row = i / 2;
        lv_coord_t x = col * 192 + (col == 0 ? 0 : 4);  // Add 4px gap between columns
        lv_coord_t y = 220 + row * 50 + (row == 0 ? 0 : 2);  // Add 2px gap between rows
        
        personCards[i] = lv_obj_create(screens[SCREEN_MAIN]);
        lv_obj_set_size(personCards[i], 186, 48);  // Reduced size for gaps (width: 188 -> 186)
        lv_obj_set_pos(personCards[i], x, y);
        lv_obj_set_style_bg_opa(personCards[i], LV_OPA_50, 0);  // 50% transparent
        lv_obj_set_style_bg_color(personCards[i], lv_color_hex(0x2a2a3e), 0);
        lv_obj_set_style_radius(personCards[i], 12, 0);
        lv_obj_set_style_border_width(personCards[i], 0, 0);
        lv_obj_clear_flag(personCards[i], LV_OBJ_FLAG_SCROLLABLE);
        
        personLabels[i] = lv_label_create(personCards[i]);
        lv_label_set_text(personLabels[i], "---");
        lv_obj_set_style_text_font(personLabels[i], &montserrat_extended_20, 0);
        lv_obj_set_style_text_color(personLabels[i], lv_color_hex(0x808080), 0);
        lv_obj_center(personLabels[i]);
    }
    
    // ========== VOICE BUTTON (Right bottom: 92x96 with 4px left gap) ==========
    voiceButton = lv_btn_create(screens[SCREEN_MAIN]);
    lv_obj_set_size(voiceButton, 92, 96);
    lv_obj_set_pos(voiceButton, 388, 224);  // Add 4px left gap (384 -> 388)
    lv_obj_set_style_bg_color(voiceButton, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_bg_color(voiceButton, lv_color_hex(0x2563eb), LV_STATE_PRESSED);
    lv_obj_set_style_radius(voiceButton, 16, 0);
    lv_obj_set_style_shadow_width(voiceButton, 15, 0);
    lv_obj_set_style_shadow_color(voiceButton, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_shadow_opa(voiceButton, LV_OPA_50, 0);
    lv_obj_set_style_border_width(voiceButton, 0, 0);
    lv_obj_add_event_cb(voiceButton, voice_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* voiceIcon = lv_label_create(voiceButton);
    lv_label_set_text(voiceIcon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(voiceIcon, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(voiceIcon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(voiceIcon);
    
    // ========== EMPTY SPACE (96x80 at 0,80) - left transparent ==========
    familyContainer = lv_obj_create(screens[SCREEN_MAIN]);
    lv_obj_set_size(familyContainer, 96, 80);
    lv_obj_set_pos(familyContainer, 0, 80);
    lv_obj_set_style_bg_opa(familyContainer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(familyContainer, 0, 0);
    lv_obj_clear_flag(familyContainer, LV_OBJ_FLAG_SCROLLABLE);
}

void LVGL_UI::createQuickActionsScreen() {
    screens[SCREEN_QUICK_ACTIONS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_QUICK_ACTIONS], lv_color_hex(0x0d0f1a), 0);
    
    // Add swipe gesture
    lv_obj_add_event_cb(screens[SCREEN_QUICK_ACTIONS], screen_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(screens[SCREEN_QUICK_ACTIONS], LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t* titleLabel = lv_label_create(screens[SCREEN_QUICK_ACTIONS]);
    lv_label_set_text(titleLabel, "Quick Actions");
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 20);
    
    quickActionsLabel = lv_label_create(screens[SCREEN_QUICK_ACTIONS]);
    lv_label_set_text(quickActionsLabel, "Coming Soon");
    lv_obj_set_style_text_font(quickActionsLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(quickActionsLabel, lv_color_hex(0x8b92a5), 0);
    lv_obj_align(quickActionsLabel, LV_ALIGN_CENTER, 0, 0);
    
    // Swipe hint
    lv_obj_t* hintLabel = lv_label_create(screens[SCREEN_QUICK_ACTIONS]);
    lv_label_set_text(hintLabel, LV_SYMBOL_LEFT " Swipe to go back");
    lv_obj_set_style_text_font(hintLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hintLabel, lv_color_hex(0x505672), 0);
    lv_obj_align(hintLabel, LV_ALIGN_BOTTOM_MID, 0, -20);
}

lv_obj_t* LVGL_UI::createPersonCard(lv_obj_t* parent, const char* name, bool present, lv_coord_t x, lv_coord_t y) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, 60, 45);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    // Green background if home, dark if away
    if (present) {
        lv_obj_set_style_bg_color(card, lv_color_hex(0x22c55e), 0);  // Green
    } else {
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a3e), 0);  // Dark
    }
    
    lv_obj_t* label = lv_label_create(card);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    
    // White text if home, gray if away
    if (present) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    } else {
        lv_obj_set_style_text_color(label, lv_color_hex(0x808080), 0);
    }
    lv_obj_center(label);
    
    return card;
}

void LVGL_UI::updateWeather(float temp, const char* condition) {
    if (!tempLabel || !conditionLabel || !weatherIcon) return;
    
    // Format temperature with degree symbol
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f°", temp);
    lv_label_set_text(tempLabel, tempStr);
    
    // Map condition to display text and icon
    const char* displayCondition = condition;
    const lv_img_dsc_t* iconImage = &clear_day;  // Default
    
    // Convert to lowercase for comparison
    String cond = String(condition);
    cond.toLowerCase();
    
    // Check for night conditions first
    bool isNight = (cond.indexOf("night") >= 0);
    
    if (cond.indexOf("sunny") >= 0 || cond.indexOf("clear") >= 0) {
        displayCondition = isNight ? "Clear" : "Sunny";
        iconImage = isNight ? &clear_night : &clear_day;
    }
    else if (cond.indexOf("partly") >= 0) {
        displayCondition = "Partly Cloudy";
        iconImage = isNight ? &partly_cloudy_night : &partly_cloudy_day;
    }
    else if (cond.indexOf("cloud") >= 0 || cond.indexOf("overcast") >= 0) {
        displayCondition = "Cloudy";
        iconImage = &cloudy;
    }
    else if (cond.indexOf("rain") >= 0 || cond.indexOf("drizzle") >= 0 || cond.indexOf("shower") >= 0) {
        displayCondition = "Rainy";
        iconImage = &rain;
    }
    else if (cond.indexOf("snow") >= 0) {
        displayCondition = "Snowy";
        iconImage = &snow;
    }
    else if (cond.indexOf("fog") >= 0 || cond.indexOf("mist") >= 0 || cond.indexOf("haz") >= 0) {
        displayCondition = "Foggy";
        iconImage = &fog;
    }
    else if (cond.indexOf("thunder") >= 0 || cond.indexOf("storm") >= 0 || cond.indexOf("lightning") >= 0) {
        displayCondition = "Stormy";
        iconImage = &thunder_rain;
    }
    else if (cond.indexOf("wind") >= 0) {
        displayCondition = "Windy";
        iconImage = &wind;
    }
    
    // Update icon image and make visible
    lv_img_set_src(weatherIcon, iconImage);
    lv_obj_clear_flag(weatherIcon, LV_OBJ_FLAG_HIDDEN);
    
    lv_label_set_text(conditionLabel, displayCondition);
    
    // Force screen update
    lv_obj_invalidate(weatherContainer);
}

void LVGL_UI::updatePersonPresence(int personIndex, const char* name, bool present, uint32_t color) {
    if (personIndex < 0 || personIndex >= MAX_PEOPLE) return;
    
    strncpy(people[personIndex].name, name, sizeof(people[personIndex].name) - 1);
    people[personIndex].present = present;
    people[personIndex].color = color;
    
    if (personIndex >= personCount) {
        personCount = personIndex + 1;
    }
    
    // Update the person card directly
    if (personCards[personIndex] && personLabels[personIndex]) {
        // Update background color: green if home, dark if away (50% transparent)
        if (present) {
            lv_obj_set_style_bg_color(personCards[personIndex], lv_color_hex(0x22c55e), 0);  // Green
            lv_obj_set_style_bg_opa(personCards[personIndex], LV_OPA_50, 0);  // 50% transparent
            lv_obj_set_style_text_color(personLabels[personIndex], lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_bg_color(personCards[personIndex], lv_color_hex(0x2a2a3e), 0);  // Dark
            lv_obj_set_style_bg_opa(personCards[personIndex], LV_OPA_50, 0);  // 50% transparent
            lv_obj_set_style_text_color(personLabels[personIndex], lv_color_hex(0x808080), 0);
        }
        lv_label_set_text(personLabels[personIndex], name);
        
        // Force screen update
        lv_obj_invalidate(personCards[personIndex]);
    }
}

void LVGL_UI::setAnyoneHome(bool isHome) {
    // This function is no longer needed with new UI design
    // Individual person cards show presence status
    (void)isHome;
}

void LVGL_UI::updateGateStatus(bool isOpen) {
    if (!gateStatusLabel || !gateIcon) return;
    
    if (isOpen) {
        lv_label_set_text(gateStatusLabel, "Open");
        lv_obj_set_style_bg_color(gateIcon, lv_color_hex(0x22c55e), 0);  // Green
    } else {
        lv_label_set_text(gateStatusLabel, "Closed");
        lv_obj_set_style_bg_color(gateIcon, lv_color_hex(0xef4444), 0);  // Red
    }
}

void LVGL_UI::showScreen(ScreenID screenId) {
    if (screenId < 0 || screenId >= SCREEN_COUNT || !screens[screenId]) return;
    
    // Instant screen switch - no animation
    lv_scr_load(screens[screenId]);
    currentScreen = screenId;
}

void LVGL_UI::setVoiceButtonCallback(void (*callback)()) {
    voiceCallback = callback;
}

// Static callbacks
void LVGL_UI::disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    instance->tft.startWrite();
    instance->tft.setAddrWindow(area->x1, area->y1, w, h);
    instance->tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    instance->tft.endWrite();
    
    lv_disp_flush_ready(disp);
}

void LVGL_UI::touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    instance->touch.loop();
    
    if (touchDetected && lastTouchX >= 0 && lastTouchY >= 0) {
        data->state = LV_INDEV_STATE_PR;
        
        // Transform coordinates for landscape mode
        data->point.x = map(lastTouchY, 0, 480, 0, 479);
        data->point.y = map(320 - lastTouchX, 0, 320, 0, 319);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void LVGL_UI::voice_button_cb(lv_event_t* e) {
    if (instance && instance->voiceCallback) {
        instance->voiceCallback();
    }
}

void LVGL_UI::screen_gesture_cb(lv_event_t* e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    
    if (dir == LV_DIR_LEFT) {
        // Swipe left - next screen
        int next = (instance->currentScreen + 1) % SCREEN_COUNT;
        instance->showScreen((ScreenID)next);
    } else if (dir == LV_DIR_RIGHT) {
        // Swipe right - previous screen
        int prev = (instance->currentScreen - 1 + SCREEN_COUNT) % SCREEN_COUNT;
        instance->showScreen((ScreenID)prev);
    }
}
