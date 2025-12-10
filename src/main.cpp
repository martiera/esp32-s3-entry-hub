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
 * - Future: LVGL touchscreen display
 * 
 * Hardware: ESP32-S3-WROOM-1
 * Microphone: INMP441 I2S
 */

#include <Arduino.h>
#include "config.h"
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

// System state
unsigned long lastStatusUpdate = 0;
unsigned long lastVoiceCheck = 0;
bool systemReady = false;

// Forward declarations
void setupSystem();
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
    wifiMgr.begin();
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
    Serial.println("✓ (Placeholder - add display hardware)");
    
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
    
    // Broadcast to WebSocket clients
    JsonDocument doc;
    doc["type"] = "status";
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["recording"] = audioHandler.isRecording();
    
    webServer.broadcastStatus(doc);
}
