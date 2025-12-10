#ifndef CONFIG_H
#define CONFIG_H

// Device Information
#define DEVICE_NAME "ESP32-EntryHub"
#define DEVICE_VERSION "1.0.0"
#define HOSTNAME "entryhub"

// Pin Definitions - I2S INMP441 Microphone
#define I2S_SCK_PIN 4
#define I2S_WS_PIN 5
#define I2S_SD_PIN 6
#define I2S_PORT I2S_NUM_0

// Pin Definitions - SPI Display (future LVGL)
#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_SCLK 12
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8
#define TFT_BL 7

// Pin Definitions - I2C Touch
#define TOUCH_SDA 17
#define TOUCH_SCL 18

// Audio Configuration
#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 16
#define AUDIO_BUFFER_SIZE 512
#define DMA_BUFFER_COUNT 4
#define DMA_BUFFER_LEN 1024

// WiFi Configuration
#define WIFI_TIMEOUT_MS 20000
#define WIFI_RECONNECT_INTERVAL 30000

// MQTT Configuration
#define MQTT_RECONNECT_INTERVAL 5000
#define MQTT_BUFFER_SIZE 2048

// Web Server
#define WEB_SERVER_PORT 80
#define WEBSOCKET_PORT 81

// File System
#define CONFIG_FILE "/config.json"
#define COMMANDS_FILE "/commands.json"
#define PRESENCE_FILE "/presence.json"

// OTA
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "admin123"
#endif

// OTA
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "admin123"
#endif

// Features
#define ENABLE_WEBSOCKET true
#define ENABLE_OTA true
#define ENABLE_MDNS true
#define ENABLE_VOICE_FEEDBACK true

// Voice Recognition
#define WAKE_WORD_SENSITIVITY 0.5f
#define COMMAND_TIMEOUT_MS 5000
#define VOICE_BUFFER_DURATION_MS 3000

// Home Assistant
#define HA_DISCOVERY_PREFIX "homeassistant"
#define HA_UPDATE_INTERVAL 30000

// Weather
#define WEATHER_UPDATE_INTERVAL 600000  // 10 minutes
#define WEATHER_LOCATION "London"

// Presence Detection
#define PRESENCE_TIMEOUT_MS 300000  // 5 minutes

#endif
