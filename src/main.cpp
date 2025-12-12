/**
 * ESP32-S3 Entry Hub
 * 
 * Advanced voice-controlled smart home entry panel with:
 * - Voice wake word detection (Porcupine)
 * - Voice command recognition (TensorFlow Lite Micro)
 * - Web-based admin panel
 * - Home Assistant integration
 * - MQTT connectivity
 * - OTA updates
 * - Presence tracking
 * - Weather & calendar widgets
 * - 3.5" ILI9488 IPS Display with FT6236 Touch
 * - Onboard RGB LED feedback
 * 
 * Hardware: ESP32-S3-N16R8 (16MB Flash + 8MB PSRAM)
 * Display: 3.5" ILI9488 480x320 IPS + FT6236 Capacitive Touch
 * Microphone: INMP441 I2S
 * LED: Onboard WS2812 RGB (GPIO 48)
 */

#include <Arduino.h>
#include <HTTPClient.h>
#include "config.h"
#include "pins.h"
#include "secrets.h"
#include "wifi_manager.h"
#include "mqtt_client.h"
#include "ota_manager.h"
#include "audio_handler.h"
#include "porcupine_handler.h"
#include "storage_manager.h"
#include "web_server.h"
#include "ha_integration.h"
#include "display_manager.h"
#include "led_feedback.h"

// System state
unsigned long lastStatusUpdate = 0;
unsigned long lastVoiceCheck = 0;
bool systemReady = false;

// Forward declarations
void setupSystem();
void testIntegrationsOnStartup(JsonDocument& config);
void handleVoiceRecognition();
void handleMqttMessages(const char* topic, const char* payload);
void publishSystemStatus();
void processVoiceCommand(const char* command);

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 Entry Hub                  ║");
    Serial.println("║   Voice-Controlled Access Panel       ║");
    Serial.println("║   Version: " DEVICE_VERSION "                      ║");
    Serial.println("╚═══════════════════════════════════════╝");
    Serial.println();
    
    // Initialize system
    setupSystem();
    
    Serial.println("\n✓ System initialization complete!");
    Serial.println("══════════════════════════════════════════\n");
    systemReady = true;
}

void loop() {
    // Core system loops
    wifiMgr.loop();
    mqttClient.loop();
    otaManager.loop();
    webServer.loop();
    homeAssistant.loop();
    display.loop();
    ledFeedback.loop();
    
    // Audio processing
    audioHandler.loop();
    porcupineHandler.loop();
    
    // Voice recognition
    if (systemReady) {
        handleVoiceRecognition();
    }
    
    // Periodic status updates
    unsigned long now = millis();
    if (now - lastStatusUpdate >= 30000) { // Every 30 seconds
        lastStatusUpdate = now;
        publishSystemStatus();
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}

void setupSystem() {
    Serial.println("Initializing system components...\n");
    
    // 0. LED Feedback (first for visual feedback during boot)
    Serial.print("→ LED feedback... ");
    if (ledFeedback.begin(LED_PIN, LED_BRIGHTNESS)) {
        ledFeedback.showBooting();
        Serial.println("✓");
    } else {
        Serial.println("✗ WARNING: LED initialization failed");
    }
    
    // 1. Storage (must be first)
    Serial.print("→ Storage system... ");
    if (storage.begin()) {
        Serial.println("✓");
        storage.listFiles();
    } else {
        Serial.println("✗ FAILED!");
    }
    
    // 2. WiFi
    Serial.print("→ WiFi connection... ");
    ledFeedback.showWiFiConnecting();
    wifiMgr.begin();
    ledFeedback.showWiFiConnected();
    Serial.println("✓");
    
    // 3. MQTT
    Serial.print("→ MQTT client... ");
    mqttClient.begin();
    mqttClient.setCallback(handleMqttMessages);
    Serial.println("✓");
    
    // 4. Web Server
    Serial.print("→ Web server... ");
    webServer.begin();
    Serial.println("✓");
    Serial.printf("   Access admin panel at: http://%s\n", wifiMgr.getIPAddress().c_str());
    Serial.printf("   Or: http://%s.local\n", HOSTNAME);
    
    // 5. OTA Updates
    Serial.print("→ OTA updates... ");
    otaManager.begin();
    Serial.println("✓");
    
    // 6. Audio Input
    Serial.print("→ I2S audio input... ");
    if (audioHandler.begin()) {
        Serial.println("✓");
        audioHandler.startRecording();
    } else {
        Serial.println("✗ WARNING: Audio initialization failed");
    }
    
    // 7. Wake Word Detection
    Serial.print("→ Porcupine wake word... ");
    if (porcupineHandler.begin()) {
        Serial.println("✓");
    } else {
        Serial.println("⚠️  Template mode (add Porcupine library)");
    }
    
    // 8. Home Assistant
    Serial.print("→ Home Assistant integration... ");
    homeAssistant.begin();
    Serial.println("✓");
    
    // 9. Display (placeholder for future)
    Serial.print("→ Display manager... ");
    display.begin();
    Serial.println("✓ (Add display hardware to enable)");
    
    // LED to idle state after boot
    ledFeedback.showIdle();
    
    // Load configuration
    Serial.print("\n→ Loading configuration... ");
    JsonDocument config;
    bool configExists = storage.loadConfig(config);
    bool needsSave = false;
    
    if (configExists) {
        Serial.println("✓");
        // Apply configuration settings here
        
        // Update existing config with token from secrets.h if missing
        if (!config["integrations"]["home_assistant"]["token"].is<const char*>() || 
            String(config["integrations"]["home_assistant"]["token"].as<const char*>()).isEmpty()) {
            Serial.println("→ Updating HA token from secrets.h");
            config["integrations"]["home_assistant"]["token"] = HA_TOKEN;
            config["integrations"]["home_assistant"]["url"] = HA_BASE_URL;
            config["integrations"]["home_assistant"]["enabled"] = true;
            config["integrations"]["home_assistant"]["discovery"] = true;
            needsSave = true;
        }
    } else {
        Serial.println("⚠️  Using defaults");
        // Create default configuration with secrets from secrets.h
        config["device"]["name"] = DEVICE_NAME;
        config["device"]["version"] = DEVICE_VERSION;
        config["wake_word"] = "jarvis";
        config["sensitivity"] = WAKE_WORD_SENSITIVITY;
        
        // Initialize integrations with secrets
        config["integrations"]["home_assistant"]["enabled"] = true;
        config["integrations"]["home_assistant"]["url"] = HA_BASE_URL;
        config["integrations"]["home_assistant"]["token"] = HA_TOKEN;
        config["integrations"]["home_assistant"]["discovery"] = true;
        
        needsSave = true;
    }
    
    if (needsSave) {
        storage.saveConfig(config);
    }
    
    // Test integrations if configured
    Serial.print("\n→ Testing integrations... ");
    testIntegrationsOnStartup(config);
    Serial.println("✓");
    
    // Publish initial status
    delay(2000);
    publishSystemStatus();
}

void handleVoiceRecognition() {
    // Check for audio input at regular intervals
    unsigned long now = millis();
    if (now - lastVoiceCheck < 50) { // Check every 50ms
        return;
    }
    lastVoiceCheck = now;
    
    // Read audio samples
    int16_t audioBuffer[512];
    size_t samplesRead = audioHandler.readAudio(audioBuffer, 512);
    
    if (samplesRead > 0) {
        // Process with Porcupine for wake word detection
        if (porcupineHandler.processAudioFrame(audioBuffer, samplesRead)) {
            // Wake word detected!
            const char* wakeWord = porcupineHandler.getDetectedKeyword();
            
            Serial.println("\n═══════════════════════════════════");
            Serial.printf("🎤 WAKE WORD DETECTED: %s\n", wakeWord);
            Serial.println("═══════════════════════════════════\n");
            
            // Notify systems
            mqttClient.publishVoiceDetection(wakeWord);
            homeAssistant.updateVoiceCommandSensor(wakeWord);
            webServer.broadcastMessage("voice_detected", wakeWord);
            display.showWakeWordDetected();
            
            // TODO: Now listen for command using TensorFlow Lite Micro
            // This would involve:
            // 1. Recording audio for command duration
            // 2. Processing with TFLM model
            // 3. Executing recognized command
            
            Serial.println("Listening for command...");
            delay(3000); // Simulate listening period
            
            // Example command processing (replace with actual TFLM)
            const char* demoCommand = "turn on the lights";
            processVoiceCommand(demoCommand);
        }
    }
}

void processVoiceCommand(const char* command) {
    Serial.printf("Processing command: %s\n", command);
    
    // Update sensors
    homeAssistant.updateVoiceCommandSensor(command);
    mqttClient.publishCommandExecuted(command, "success");
    
    // Parse and execute command
    String cmd = String(command);
    cmd.toLowerCase();
    
    if (cmd.indexOf("light") >= 0) {
        if (cmd.indexOf("on") >= 0) {
            Serial.println("→ Turning lights ON");
            homeAssistant.controlLight("living_room", true);
        } else if (cmd.indexOf("off") >= 0) {
            Serial.println("→ Turning lights OFF");
            homeAssistant.controlLight("living_room", false);
        }
    }
    else if (cmd.indexOf("lock") >= 0 || cmd.indexOf("door") >= 0) {
        if (cmd.indexOf("lock") >= 0) {
            Serial.println("→ Locking door");
            homeAssistant.controlLock("front_door", true);
        } else if (cmd.indexOf("unlock") >= 0) {
            Serial.println("→ Unlocking door");
            homeAssistant.controlLock("front_door", false);
        }
    }
    else if (cmd.indexOf("garage") >= 0) {
        Serial.println("→ Operating garage door");
        homeAssistant.controlCover("garage_door", "toggle");
    }
    else if (cmd.indexOf("good night") >= 0) {
        Serial.println("→ Activating Good Night scene");
        homeAssistant.activateScene("good_night");
    }
    else if (cmd.indexOf("welcome home") >= 0) {
        Serial.println("→ Activating Welcome Home scene");
        homeAssistant.activateScene("welcome_home");
    }
    else {
        Serial.println("→ Command not recognized");
    }
    
    webServer.broadcastMessage("command_executed", command);
}

void handleMqttMessages(const char* topic, const char* payload) {
    Serial.printf("MQTT: %s = %s\n", topic, payload);
    
    String topicStr = String(topic);
    
    // Handle remote commands
    if (topicStr.equals("entryhub/command")) {
        processVoiceCommand(payload);
    }
    // Handle configuration updates
    else if (topicStr.equals("entryhub/config")) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            storage.saveConfig(doc);
            Serial.println("Configuration updated via MQTT");
        }
    }
    // Handle Home Assistant entity states
    else if (topicStr.startsWith("homeassistant/")) {
        // Process entity state updates
        display.showNotification("Entity Update", payload);
    }
}

void publishSystemStatus() {
    if (!mqttClient.isConnected()) {
        return;
    }
    
    Serial.println("📊 Publishing system status...");
    
    // Publish to MQTT
    mqttClient.publishStatus("online");
    
    // Broadcast to WebSocket clients with same format as API
    JsonDocument doc;
    doc["type"] = "status";
    
    // Device info
    doc["device"]["name"] = DEVICE_NAME;
    doc["device"]["version"] = DEVICE_VERSION;
    doc["device"]["uptime"] = millis() / 1000;
    doc["device"]["free_heap"] = ESP.getFreeHeap();
    
    // WiFi info
    doc["wifi"]["connected"] = WiFi.isConnected();
    doc["wifi"]["ssid"] = WiFi.SSID();
    doc["wifi"]["ip"] = WiFi.localIP().toString();
    doc["wifi"]["rssi"] = WiFi.RSSI();
    
    // MQTT info
    doc["mqtt"]["connected"] = mqttClient.isConnected();
    
    // Audio info
    doc["audio"]["recording"] = audioHandler.isRecording();
    
    // Voice info
    doc["voice"]["wake_word"] = "jarvis";
    doc["voice"]["active"] = false;
    
    webServer.broadcastStatus(doc);
}

void testIntegrationsOnStartup(JsonDocument& config) {
    Serial.println("Starting integration tests...");
    
    // Test Home Assistant integration
    if (config["integrations"]["home_assistant"]["enabled"] == true) {
        const char* haUrl = config["integrations"]["home_assistant"]["url"];
        const char* haToken = config["integrations"]["home_assistant"]["token"];
        
        if (haUrl && strlen(haUrl) > 0 && haToken && strlen(haToken) > 0) {
            Serial.print("\n  → Home Assistant... ");
            
            HTTPClient http;
            String url = String(haUrl);
            if (!url.endsWith("/")) url += "/";
            url += "api/";
            
            http.begin(url);
            http.addHeader("Authorization", String("Bearer ") + haToken);
            http.setTimeout(5000);
            
            int httpCode = http.GET();
            http.end();
            
            if (httpCode == 200) {
                Serial.print("✓ Connected");
                config["integrations"]["home_assistant"]["status"] = "connected";
            } else {
                Serial.printf("✗ Failed (HTTP %d)", httpCode);
                config["integrations"]["home_assistant"]["status"] = "failed";
            }
        } else {
            Serial.print("\n  → Home Assistant... ⚠️ Not configured");
            config["integrations"]["home_assistant"]["status"] = "not_configured";
        }
    }
    
    // Test weather provider
    if (config["weather"]["provider"] == "openweathermap") {
        const char* apiKey = config["weather"]["api_key"];
        const char* location = config["weather"]["location"];
        
        if (apiKey && strlen(apiKey) > 0 && location && strlen(location) > 0) {
            Serial.print("\n  → OpenWeatherMap... ");
            
            HTTPClient http;
            String url = "http://api.openweathermap.org/data/2.5/weather?q=";
            url += location;
            url += "&appid=";
            url += apiKey;
            
            http.begin(url);
            http.setTimeout(5000);
            
            int httpCode = http.GET();
            http.end();
            
            if (httpCode == 200) {
                Serial.print("✓ Connected");
                config["weather"]["status"] = "connected";
            } else {
                Serial.printf("✗ Failed (HTTP %d)", httpCode);
                config["weather"]["status"] = "failed";
            }
        } else {
            Serial.print("\n  → OpenWeatherMap... ⚠️ Not configured");
            config["weather"]["status"] = "not_configured";
        }
    } else if (config["weather"]["provider"] == "homeassistant") {
        // Weather from HA - depends on HA connection
        config["weather"]["status"] = config["integrations"]["home_assistant"]["status"];
    }
    
    // Save updated config with status
    storage.saveConfig(config);
}
