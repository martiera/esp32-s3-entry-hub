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
#include "voice_activity_handler.h"
#include "storage_manager.h"
#include "web_server.h"
#include "ha_integration.h"
#include "ha_assist_client.h"
#include "display_manager.h"
#include "led_feedback.h"
#include "notification_manager.h"

// System state
unsigned long lastStatusUpdate = 0;
unsigned long lastVoiceCheck = 0;
bool systemReady = false;

// Voice command state
bool isListeningForCommand = false;
unsigned long listeningStartTime = 0;
const unsigned long LISTENING_TIMEOUT_MS = 1200; // 1.2 seconds to speak command

// Forward declarations
void setupSystem();
void testIntegrationsOnStartup(JsonDocument& config);
void handleVoiceRecognition();
void handleMqttMessages(const char* topic, const char* payload);
void publishSystemStatus();
void processVoiceCommand(const char* command);
void onAssistResult(const char* transcription, const char* response, const char* error);

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
    notificationManager.loop();
    
    // Audio processing
    audioHandler.loop();
    voiceActivity.loop();
    
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
    
    // 2.5. NTP Time Sync
    Serial.print("→ NTP time sync... ");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    // Wait for time to be set (max 10 seconds)
    time_t now = 0;
    int retry = 0;
    while (now < 1000000000 && retry < 20) {
        delay(500);
        time(&now);
        retry++;
    }
    if (now > 1000000000) {
        struct tm* timeinfo = localtime(&now);
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
        Serial.printf("✓ (%s)\n", timeStr);
    } else {
        Serial.println("✗ WARNING: Time sync failed");
    }
    
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
        // Quick microphone test
        audioHandler.testMicrophone();
        audioHandler.startRecording();
    } else {
        Serial.println("✗ WARNING: Audio initialization failed");
    }
    
    // 7. Voice Activity Detection
    Serial.print("→ Voice activity detection... ");
    if (voiceActivity.begin()) {
        Serial.println("✓");
    } else {
        Serial.println("✗ Failed");
    }
    
    // 8. Home Assistant
    Serial.print("→ Home Assistant integration... ");
    homeAssistant.begin();
    Serial.println("✓");
    
    // 8.5. HA Assist Client (voice-to-text pipeline)
    Serial.print("→ HA Assist client... ");
    haAssist.begin(HA_BASE_URL, HA_TOKEN);
    haAssist.setResultCallback(onAssistResult);
    haAssist.setLanguage("en");
    Serial.println("✓");
    Serial.printf("   Assist endpoint: %s\n", HA_BASE_URL);
    
    // 9. Display
    Serial.print("→ Display manager... ");
    display.begin();
    Serial.println("✓");
    
    // 10. Notification Manager
    Serial.print("→ Notification manager... ");
    notificationManager.begin();
    Serial.println("✓");
    
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
    
    // Show dashboard on display
    delay(1000);
    display.showDashboard();
    
    // Update display with WiFi status
    if (wifiMgr.isConnected()) {
        display.updateWiFiStatus(true, wifiMgr.getRSSI());
    }
    
    // Publish initial status
    delay(2000);
    publishSystemStatus();
}

// Callback for HA Assist results
void onAssistResult(const char* transcription, const char* response, const char* error) {
    isListeningForCommand = false;
    ledFeedback.showIdle();
    
    if (error) {
        Serial.printf("❌ Assist error: %s\n", error);
        display.showNotification("Error", error);
        return;
    }
    
    if (!transcription || strlen(transcription) == 0) {
        Serial.println("⚠️  No transcription received");
        display.showNotification("Error", "No speech detected");
        return;
    }
    
    Serial.println("\n═══════════════════════════════════");
    Serial.println("🗣️  SPEECH RECOGNIZED");
    Serial.printf("You said: \"%s\"\n", transcription);
    Serial.println("═══════════════════════════════════\n");
    
    // Update HA sensors
    homeAssistant.updateVoiceCommandSensor(transcription);
    
    // Process command locally on ESP32
    processVoiceCommand(transcription);
    
    // Show on display
    display.showNotification("Voice Command", transcription);
}

void handleVoiceRecognition() {
    // Check for audio input at regular intervals
    unsigned long now = millis();
    
    // During recording, check more frequently (every 10ms) to capture all audio
    // Otherwise check every 50ms for voice activity detection
    unsigned long checkInterval = isListeningForCommand ? 10 : 50;
    
    if (now - lastVoiceCheck < checkInterval) {
        return;
    }
    lastVoiceCheck = now;
    
    // Read audio samples
    int16_t audioBuffer[512];
    size_t samplesRead = audioHandler.readAudio(audioBuffer, 512);
    
    if (samplesRead == 0) {
        return;
    }
    
    // If we're listening for a command, feed audio to HA Assist
    if (isListeningForCommand) {
        haAssist.feedAudio(audioBuffer, samplesRead);
        
        unsigned long elapsed = now - listeningStartTime;
        
        // Check for timeout or silence (end of speech)
        if (elapsed >= LISTENING_TIMEOUT_MS) {
            Serial.printf("⏱️  Listening timeout reached at %lums (target: %lums), processing...\n", 
                         elapsed, LISTENING_TIMEOUT_MS);
            ledFeedback.showProcessing();
            display.showNotification("Voice", "Processing...");
            haAssist.stopAndProcess();
            isListeningForCommand = false;
        }
        return;
    }
    
    // Process for voice activity detection (wake trigger)
    if (voiceActivity.processAudioFrame(audioBuffer, samplesRead)) {
        // Voice activity detected - start listening!
        
        Serial.println("\n═══════════════════════════════════");
        Serial.println("🎤 VOICE ACTIVITY DETECTED!");
        Serial.printf("Audio level: %ld\n", voiceActivity.getLastAudioLevel());
        Serial.println("Waiting for command...");
        Serial.println("═══════════════════════════════════\n");
        
        // Notify systems
        mqttClient.publishVoiceDetection("voice_activity");
        webServer.broadcastMessage("voice_detected", "listening");
        display.showWakeWordDetected();
        ledFeedback.showListening();
        
        // Small delay to let person take a breath before speaking command
        delay(250);
        
        Serial.println("Now listening for command...");
        
        // Start recording for HA Assist
        isListeningForCommand = true;
        listeningStartTime = millis();  // Use current time after delay
        haAssist.startRecording();
        
        // Don't feed the wake audio - start fresh after the delay
    }
}

// Normalize command text to handle common speech recognition errors
String normalizeCommand(const char* rawCommand) {
    String cmd = String(rawCommand);
    cmd.toLowerCase();
    cmd.trim();
    
    // Common mishearings for "gate"
    cmd.replace("open date", "open gate");
    cmd.replace("open this", "open gate");
    cmd.replace("open gate", "open gate");
    cmd.replace("open the gate", "open gate");
    cmd.replace("opened", "open gate");
    cmd.replace("opengate", "open gate");
    
    // Common mishearings for "garage"
    cmd.replace("garage door", "garage");
    cmd.replace("the garage", "garage");
    
    // Clean up extra spaces
    while (cmd.indexOf("  ") >= 0) {
        cmd.replace("  ", " ");
    }
    
    return cmd;
}

void processVoiceCommand(const char* command) {
    Serial.printf("Raw command: %s\n", command);
    
    // Normalize the command first
    String normalized = normalizeCommand(command);
    Serial.printf("Normalized: %s\n", normalized.c_str());
    
    // Update sensors with original command
    homeAssistant.updateVoiceCommandSensor(command);
    mqttClient.publishCommandExecuted(command, "success");
    
    // Parse and execute normalized command
    String cmd = normalized;
    
    if (cmd.indexOf("light") >= 0) {
        if (cmd.indexOf("on") >= 0) {
            Serial.println("→ Turning lights ON");
            homeAssistant.controlLight("living_room", true);
        } else if (cmd.indexOf("off") >= 0) {
            Serial.println("→ Turning lights OFF");
            homeAssistant.controlLight("living_room", false);
        }
    }
    else if (cmd.indexOf("gate") >= 0 || cmd.indexOf("garage") >= 0) {
        if (cmd.indexOf("open") >= 0) {
            Serial.println("→ Opening gate/garage");
            homeAssistant.controlCover("garage_door", "open");
        } else if (cmd.indexOf("close") >= 0) {
            Serial.println("→ Closing gate/garage");
            homeAssistant.controlCover("garage_door", "close");
        } else {
            Serial.println("→ Toggling gate/garage");
            homeAssistant.controlCover("garage_door", "toggle");
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
