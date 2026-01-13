#ifndef CONFIG_H
#define CONFIG_H

#include "pins.h"

// ============================================
// Device Information
// ============================================
#define DEVICE_NAME "ESP32-EntryHub"
#define DEVICE_VERSION "1.0.0"
#define HOSTNAME "entryhub"

// ============================================
// Hardware Configuration
// ============================================

// ESP32-S3 N16R8 Features
#define HAS_PSRAM           true
#define PSRAM_SIZE          (8 * 1024 * 1024)  // 8MB

// Display Configuration (3.5" ILI9488 480x320)
#define DISPLAY_ENABLED     true
#define DISPLAY_WIDTH       480
#define DISPLAY_HEIGHT      320
#define DISPLAY_ROTATION    1       // Landscape
#define DISPLAY_DRIVER      "ILI9488"

// Touch Configuration (FT6236)
#define TOUCH_ENABLED       true
#define TOUCH_I2C_ADDR      0x38
#define TOUCH_THRESHOLD     40

// LED Feedback Configuration
#define LED_ENABLED         true
#define LED_BRIGHTNESS      128     // 0-255 (50%)

// Speaker Configuration (disabled by default)
#define SPEAKER_ENABLED     false

// ============================================
// Audio Configuration (INMP441)
// ============================================
#define SAMPLE_RATE         16000
#define BITS_PER_SAMPLE     16
#define AUDIO_BUFFER_SIZE   512
#define DMA_BUFFER_COUNT    4
#define DMA_BUFFER_LEN      1024

// Legacy pin defines for compatibility
#define I2S_SCK_PIN     I2S_MIC_SCK
#define I2S_WS_PIN      I2S_MIC_WS
#define I2S_SD_PIN      I2S_MIC_SD
#define I2S_PORT        I2S_MIC_PORT

// ============================================
// Network Configuration
// ============================================
#define WIFI_TIMEOUT_MS         20000
#define WIFI_RECONNECT_INTERVAL 30000
#define WIFI_MAX_RECONNECT_FAILURES 10  // Reboot after 10 failed attempts (~5 minutes)

// MQTT
#define MQTT_RECONNECT_INTERVAL 5000
#define MQTT_BUFFER_SIZE        2048

// Web Server
#define WEB_SERVER_PORT     80
#define WEBSOCKET_PORT      81

// ============================================
// File System
// ============================================
#define CONFIG_FILE         "/config.json"
#define COMMANDS_FILE       "/commands.json"
#define PRESENCE_FILE       "/presence.json"

// ============================================
// Security
// ============================================
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "admin123"
#endif

// ============================================
// Features Enable/Disable
// ============================================
#define ENABLE_WEBSOCKET        true
#define ENABLE_OTA              true
#define ENABLE_MDNS             true
#define ENABLE_VOICE_FEEDBACK   true
#define ENABLE_DISPLAY          true
#define ENABLE_TOUCH            true
#define ENABLE_LED_FEEDBACK     true

// ============================================
// Voice Recognition
// ============================================
#define WAKE_WORD_SENSITIVITY   0.3f    // Lower = requires louder speech (0.0-1.0)
#define COMMAND_TIMEOUT_MS      5000
#define VOICE_BUFFER_DURATION_MS 3000

// ============================================
// Home Assistant
// ============================================
#define HA_DISCOVERY_PREFIX     "homeassistant"
#define HA_UPDATE_INTERVAL      30000

// ============================================
// Weather
// ============================================
#define WEATHER_UPDATE_INTERVAL 600000  // 10 minutes
#define WEATHER_LOCATION        "London"

// ============================================
// Presence Detection
// ============================================
#define PRESENCE_TIMEOUT_MS     300000  // 5 minutes

// ============================================
// Display Settings
// ============================================
#define DISPLAY_SLEEP_TIMEOUT   60000   // 1 minute
#define DISPLAY_AUTO_SLEEP      true
#define DISPLAY_BRIGHTNESS_MIN  10
#define DISPLAY_BRIGHTNESS_MAX  255

// ============================================
// LVGL Configuration (when display enabled)
// ============================================
// Note: LV_MEM_SIZE is defined in lv_conf.h
#ifdef HAS_PSRAM
    #define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * 40)  // 40 lines buffer
#else
    #define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * 10)  // 10 lines buffer
#endif

#endif
