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
#include "lvgl_ui.h"
#include "led_feedback.h"
#include "notification_manager.h"

// System state
unsigned long lastStatusUpdate = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastPresenceUpdate = 0;
unsigned long lastCalendarUpdate = 0;
unsigned long lastTimezoneUpdate = 0;
bool systemReady = false;
bool timezoneSet = false;

// Voice recording state machine
enum VoiceState {
    VOICE_IDLE,              // Waiting for trigger (loud sound)
    VOICE_WAITING_SPEECH,    // Triggered, waiting for speech to start
    VOICE_RECORDING,         // Recording speech
    VOICE_PROCESSING         // Sending to STT
};

VoiceState voiceState = VOICE_IDLE;
unsigned long voiceStateStartTime = 0;
unsigned long lastSpeechTime = 0;        // Last time speech was detected
unsigned long silenceStartTime = 0;      // When silence started

// Voice recording configuration
const unsigned long TRIGGER_COOLDOWN_MS = 300;          // Wait for trigger sound to fade before detecting speech
const unsigned long WAIT_FOR_SPEECH_TIMEOUT_MS = 3000;  // 3 sec to start speaking (after cooldown)
const unsigned long SILENCE_DURATION_MS = 700;          // 700ms silence = end of speech
const unsigned long MIN_RECORDING_MS = 500;             // Minimum recording duration
const unsigned long MAX_RECORDING_MS = 10000;           // Maximum 10 seconds
const int16_t SPEECH_THRESHOLD = 500;                   // Audio level to consider as speech (16-bit samples)
const int16_t SILENCE_THRESHOLD = 100;                  // Below this = silence (noise floor is ~20-30)
const int16_t MIN_SPEECH_LEVEL = 300;                   // Minimum max level during RECORDING to send to STT

// Popup auto-hide
unsigned long popupHideTime = 0;
bool popupShouldAutoHide = false;

// Audio level monitoring
int32_t totalAudioSamples = 0;
int32_t maxAudioLevel = 0;
int64_t sumAbsAudioLevel = 0;
unsigned long lastAudioLevelLog = 0;

// Forward declarations
void setupSystem();
void testIntegrationsOnStartup(JsonDocument& config);
void handleVoiceRecognition();
void handleMqttMessages(const char* topic, const char* payload);
void publishSystemStatus();
void processVoiceCommand(const char* command);
void onAssistResult(const char* transcription, const char* response, const char* error);
void updateWeatherDisplay();
void updatePresenceDisplay();
void updateCalendarDisplay();
void setTimezoneFromHA();


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
    // During voice recording, prioritize audio capture over UI updates
    // LVGL updates can take 20-50ms and starve audio sampling
    bool isVoiceActive = (voiceState == VOICE_WAITING_SPEECH || voiceState == VOICE_RECORDING);
    
    if (isVoiceActive) {
        // Fast path: only do audio capture and minimal processing
        handleVoiceRecognition();
        haAssist.loop();  // Needed for state management
        ledFeedback.loop();  // Visual feedback
        
        // Still need some LVGL ticks for the popup, but less frequently
        static unsigned long lastLvglTick = 0;
        if (millis() - lastLvglTick >= 100) {  // LVGL every 100ms during recording
            lvglUI.loop();
            lastLvglTick = millis();
        }
        return;
    }
    
    // Core system loops (normal path when not recording)
    wifiMgr.loop();
    mqttClient.loop();
    otaManager.loop();
    webServer.loop();
    homeAssistant.loop();
    haAssist.loop();
    lvglUI.loop();
    ledFeedback.loop();
    notificationManager.loop();
    
    // Audio processing for voice activity detection
    audioHandler.loop();
    voiceActivity.loop();
    
    // Voice recognition
    if (systemReady) {
        handleVoiceRecognition();
    }
    
    // Auto-hide voice popup after timeout
    if (popupShouldAutoHide && millis() >= popupHideTime) {
        log_i("⏱️ Auto-hiding popup: now=%lu hideTime=%lu", millis(), popupHideTime);
        lvglUI.hideVoicePopup();
        popupShouldAutoHide = false;
    }
    
    // Periodic status updates
    unsigned long now = millis();
    if (now - lastStatusUpdate >= 30000) { // Every 30 seconds
        lastStatusUpdate = now;
        publishSystemStatus();
    }
    
    // Periodic weather updates
    if (now - lastWeatherUpdate >= 300000) { // Every 5 minutes
        lastWeatherUpdate = now;
        updateWeatherDisplay();
    }
    
    // Periodic presence updates
    if (now - lastPresenceUpdate >= 30000) { // Every 30 seconds
        lastPresenceUpdate = now;
        updatePresenceDisplay();
    }
    
    // Periodic calendar updates
    if (now - lastCalendarUpdate >= 600000) { // Every 10 minutes
        lastCalendarUpdate = now;
        updateCalendarDisplay();
    }
    
    // Timezone update (retry until successful, then check daily)
    unsigned long timezoneInterval = timezoneSet ? 86400000 : 60000; // Daily if set, every 1 min if not
    if (now - lastTimezoneUpdate >= timezoneInterval) {
        lastTimezoneUpdate = now;
        setTimezoneFromHA();
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
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // Start with UTC
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
    
    // Fetch timezone from Home Assistant
    Serial.print("→ Setting timezone from HA... ");
    setTimezoneFromHA();
    Serial.println("✓");
    
    // 8.5. HA Assist Client (voice-to-text pipeline)
    Serial.print("→ HA Assist client... ");
    haAssist.begin(HA_BASE_URL, HA_TOKEN);
    haAssist.setResultCallback(onAssistResult);
    haAssist.setLanguage("en");
    Serial.println("✓");
    Serial.printf("   Assist endpoint: %s\n", HA_BASE_URL);
    
    // 9. Display
    Serial.print("→ LVGL Display... ");
    lvglUI.begin();
    
    // Pre-create voice popup during setup so it's ready instantly (avoids blocking during recording)
    lvglUI.showVoicePopup("", "");  // Create with empty text
    lvglUI.hideVoicePopup();  // Hide it immediately
    
    lvglUI.setVoiceButtonCallback([]() {
        if (voiceState == VOICE_IDLE && systemReady) {
            log_i("🎙️ Voice button pressed - waiting for speech");
            
            // Clear auto-hide flag AND timer immediately
            popupShouldAutoHide = false;
            popupHideTime = 0;
            
            // Start in WAITING_SPEECH state (same as voice trigger)
            voiceState = VOICE_WAITING_SPEECH;
            voiceStateStartTime = millis();
            silenceStartTime = 0;
            
            // Reset audio monitoring
            totalAudioSamples = 0;
            maxAudioLevel = 0;
            sumAbsAudioLevel = 0;
            lastAudioLevelLog = millis();
            
            // Start recording
            haAssist.startRecording();
            
            // Show popup and LED
            lvglUI.showVoicePopup("Listening...", "Speak now");
            ledFeedback.showListening();
        }
    });
    Serial.println("✓");
    
    // Initial data fetch for display
    Serial.print("→ Fetching initial data... ");
    updateWeatherDisplay();
    updatePresenceDisplay();
    updateCalendarDisplay();
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
        
        // Load voice sensitivity from config
        if (config["voice"]["sensitivity"].is<float>()) {
            float sensitivity = config["voice"]["sensitivity"].as<float>();
            voiceActivity.setSensitivity(sensitivity);
            Serial.printf("   Voice sensitivity: %.2f\n", sensitivity);
        }
        
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
        config["voice"]["sensitivity"] = WAKE_WORD_SENSITIVITY;
        
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
    // display.showDashboard();  // Disabled - using LVGL
    
    // Update display with WiFi status
    if (wifiMgr.isConnected()) {
        // display.updateWiFiStatus(true, wifiMgr.getRSSI());  // Disabled - using LVGL
    }
    
    // Publish initial status
    delay(2000);
    publishSystemStatus();
    
    // Initial weather fetch
    Serial.print("→ Fetching initial weather... ");
    updateWeatherDisplay();
    lastWeatherUpdate = millis();
    Serial.println("✓");
    
    // Load and display presence data
    Serial.print("→ Loading presence data... ");
    updatePresenceDisplay();
    lastPresenceUpdate = millis();
    Serial.println("✓");
}

// Callback for HA Assist results
void onAssistResult(const char* transcription, const char* response, const char* error) {
    voiceState = VOICE_IDLE;  // Reset state machine
    ledFeedback.showIdle();
    
    if (error) {
        Serial.printf("❌ Assist error: %s\n", error);
        lvglUI.showVoicePopup("Error", error);
        popupHideTime = millis() + 3000;
        popupShouldAutoHide = true;
        return;
    }
    
    if (!transcription || strlen(transcription) == 0) {
        Serial.println("⚠️  No transcription received (empty text but success response)");
        lvglUI.hideVoicePopup();
        return;
    }
    
    // Trim leading/trailing whitespace (Whisper often adds leading space)
    String trimmedTranscription = String(transcription);
    trimmedTranscription.trim();
    
    Serial.println("\n═══════════════════════════════════");
    Serial.println("🗣️  SPEECH RECOGNIZED");
    Serial.printf("You said: \"%s\"\n", trimmedTranscription.c_str());
    Serial.println("═══════════════════════════════════\n");
    
    // Update popup with recognized text in large font
    lvglUI.updateVoicePopupText(trimmedTranscription.c_str(), "");
    
    // Update HA sensors
    homeAssistant.updateVoiceCommandSensor(trimmedTranscription.c_str());
    
    // Process command locally on ESP32
    processVoiceCommand(trimmedTranscription.c_str());
    
    // Schedule popup to hide after 3 seconds
    popupHideTime = millis() + 3000;
    popupShouldAutoHide = true;
}

// Helper to calculate max audio level in buffer
int16_t getMaxAudioLevel(int16_t* buffer, size_t samples) {
    int16_t maxLevel = 0;
    for (size_t i = 0; i < samples; i++) {
        int16_t absVal = abs(buffer[i]);
        if (absVal > maxLevel) maxLevel = absVal;
    }
    return maxLevel;
}

void handleVoiceRecognition() {
    unsigned long now = millis();
    
    // Read audio samples (always, for both detection and recording)
    int16_t audioBuffer[512];
    size_t samplesRead = audioHandler.readAudio(audioBuffer, 512);
    
    if (samplesRead == 0) {
        return;
    }
    
    // Calculate current audio level
    int16_t currentLevel = getMaxAudioLevel(audioBuffer, samplesRead);
    
    switch (voiceState) {
        case VOICE_IDLE: {
            // Check for voice activity trigger (loud sound)
            if (voiceActivity.processAudioFrame(audioBuffer, samplesRead)) {
                // Triggered! Start waiting for speech
                voiceState = VOICE_WAITING_SPEECH;
                voiceStateStartTime = now;
                
                // Show popup immediately
                lvglUI.showVoicePopup("Listening...", "Speak now");
                ledFeedback.showListening();
                
                // Reset audio monitoring
                totalAudioSamples = 0;
                maxAudioLevel = 0;
                sumAbsAudioLevel = 0;
                lastAudioLevelLog = now;
                
                // Start recording (to capture from beginning)
                haAssist.startRecording();
                
                Serial.println("\n═══════════════════════════════════");
                Serial.println("🎤 TRIGGERED! Waiting for speech...");
                Serial.printf("Trigger level: %ld\n", voiceActivity.getLastAudioLevel());
                Serial.println("═══════════════════════════════════\n");
                
                mqttClient.publishVoiceDetection("voice_activity");
                webServer.broadcastMessage("voice_detected", "listening");
            }
            break;
        }
        
        case VOICE_WAITING_SPEECH: {
            // Feed audio to recorder (capturing everything from trigger)
            haAssist.feedAudio(audioBuffer, samplesRead);
            totalAudioSamples += samplesRead;
            
            // Don't track maxAudioLevel here - only track during actual RECORDING
            // The trigger spike would inflate the max and cause false positives
            
            unsigned long timeSinceTrigger = now - voiceStateStartTime;
            
            // Cooldown period: ignore audio levels while trigger sound fades out
            if (timeSinceTrigger < TRIGGER_COOLDOWN_MS) {
                // Still in cooldown, don't check for speech yet
                // This prevents the trigger sound itself from being detected as speech
                break;
            }
            
            // After cooldown, check if speech started (audio above threshold)
            if (currentLevel > SPEECH_THRESHOLD) {
                voiceState = VOICE_RECORDING;
                lastSpeechTime = now;
                silenceStartTime = 0;
                
                // Reset maxAudioLevel for RECORDING phase only
                // (don't count the initial trigger spike)
                maxAudioLevel = currentLevel;
                
                Serial.printf("🗣️  Speech detected! Level: %d, starting recording (after %.1fs cooldown)...\n", 
                             currentLevel, timeSinceTrigger / 1000.0f);
            }
            
            // Timeout - no speech detected, cancel without sending to STT
            if (timeSinceTrigger > (TRIGGER_COOLDOWN_MS + WAIT_FOR_SPEECH_TIMEOUT_MS)) {
                Serial.println("⏱️  Timeout waiting for speech (3s + cooldown), cancelling...");
                
                haAssist.cancelRecording();  // Discard audio, don't send to STT
                lvglUI.hideVoicePopup();
                ledFeedback.showIdle();
                voiceState = VOICE_IDLE;
            }
            break;
        }
        
        case VOICE_RECORDING: {
            // Feed audio to recorder
            haAssist.feedAudio(audioBuffer, samplesRead);
            totalAudioSamples += samplesRead;
            
            // Track max level for stats
            if (currentLevel > maxAudioLevel) maxAudioLevel = currentLevel;
            
            // Log progress every 200ms
            if (now - lastAudioLevelLog >= 200) {
                float duration = (float)totalAudioSamples / 16000.0f;
                Serial.printf("🎤 Recording: %.1fs, level=%d, max=%ld\n", duration, currentLevel, maxAudioLevel);
                lastAudioLevelLog = now;
            }
            
            // Check for speech vs silence
            if (currentLevel > SILENCE_THRESHOLD) {
                // Still speaking (or at least some audio)
                lastSpeechTime = now;
                silenceStartTime = 0;
            } else {
                // Silence detected (below noise floor)
                if (silenceStartTime == 0) {
                    silenceStartTime = now;
                }
            }
            
            unsigned long recordingDuration = now - voiceStateStartTime;
            unsigned long silenceDuration = (silenceStartTime > 0) ? (now - silenceStartTime) : 0;
            
            // End recording conditions:
            // 1. Silence detected for SILENCE_DURATION_MS (and min recording met)
            // 2. Max recording duration reached
            bool silenceEnd = (silenceDuration >= SILENCE_DURATION_MS) && (recordingDuration >= MIN_RECORDING_MS);
            bool maxTimeEnd = (recordingDuration >= MAX_RECORDING_MS);
            
            if (silenceEnd || maxTimeEnd) {
                float duration = (float)totalAudioSamples / 16000.0f;
                
                if (silenceEnd) {
                    Serial.printf("🔇 Silence detected, ending recording (%.1fs, %ld samples, max=%ld)\n", duration, totalAudioSamples, maxAudioLevel);
                } else {
                    Serial.printf("⏱️  Max recording time reached (%.1fs, %ld samples, max=%ld)\n", duration, totalAudioSamples, maxAudioLevel);
                }
                
                // Check if recording had actual speech (not just silence)
                if (maxAudioLevel < MIN_SPEECH_LEVEL) {
                    Serial.printf("⚠️  No speech detected in recording (max level %ld < %d), cancelling...\n", maxAudioLevel, MIN_SPEECH_LEVEL);
                    haAssist.cancelRecording();  // Discard audio, don't send to STT
                    lvglUI.hideVoicePopup();
                    ledFeedback.showIdle();
                    voiceState = VOICE_IDLE;
                } else if (totalAudioSamples < 3200) {  // Less than 0.2s
                    Serial.println("⚠️  Recording too short, cancelling...");
                    haAssist.cancelRecording();  // Discard audio, don't send to STT
                    lvglUI.hideVoicePopup();
                    ledFeedback.showIdle();
                    voiceState = VOICE_IDLE;
                } else {
                    // Process the recording
                    voiceState = VOICE_PROCESSING;
                    lvglUI.updateVoicePopupText("Processing...", "");
                    ledFeedback.showProcessing();
                    haAssist.stopAndProcess();
                }
            }
            break;
        }
        
        case VOICE_PROCESSING: {
            // Just wait for callback - haAssist handles processing
            // State will be reset in onAssistResult callback
            break;
        }
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
        // display.showNotification("Entity Update", payload);
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

void updateWeatherDisplay() {
    // Fetch weather from Home Assistant
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        log_e("Failed to load config for weather");
        return;
    }
    
    const char* provider = config["weather"]["provider"] | "none";
    if (strcmp(provider, "homeassistant") != 0) {
        log_i("Weather provider not Home Assistant, skipping");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    const char* entityId = config["weather"]["home_assistant"]["entity_id"] | "weather.forecast_home";
    
    if (!haUrl || strlen(haUrl) == 0 || !haToken || strlen(haToken) == 0) {
        log_e("Home Assistant not configured for weather");
        return;
    }
    
    // Build URL
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/states/";
    url += entityId;
    
    // Make HTTP request
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + haToken);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        JsonDocument haDoc;
        DeserializationError error = deserializeJson(haDoc, payload);
        
        if (!error) {
            float temp = haDoc["attributes"]["temperature"] | 0.0f;
            const char* state = haDoc["state"] | "unknown";
            
            log_i("Weather updated: %.1f°C, %s", temp, state);
            
            // Only update if we have valid data
            if (temp != 0.0f || strcmp(state, "unknown") != 0) {
                lvglUI.updateWeather(temp, state);
            } else {
                log_w("Weather data incomplete, skipping update");
            }
        } else {
            log_e("Failed to parse weather JSON: %s", error.c_str());
        }
    } else {
        http.end();
        log_e("Failed to fetch weather from HA: HTTP %d", httpCode);
    }
}

void updatePresenceDisplay() {
    // Load config to get HA settings
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        log_e("Failed to load config for presence");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    
    if (!haUrl || strlen(haUrl) == 0 || !haToken || strlen(haToken) == 0) {
        log_e("Home Assistant not configured for presence");
        return;
    }
    
    // Check if person tracking is configured
    if (!config["presence"]["home_assistant"]["entity_ids"].is<JsonArray>()) {
        log_w("No person entities configured");
        return;
    }
    
    JsonArray entityIds = config["presence"]["home_assistant"]["entity_ids"].as<JsonArray>();
    if (entityIds.size() == 0) {
        log_w("No person entities configured");
        return;
    }
    
    // Fetch each person entity from Home Assistant
    int personIndex = 0;
    for (JsonVariant entityId : entityIds) {
        if (personIndex >= MAX_PEOPLE) break;
        
        String url = String(haUrl);
        if (!url.endsWith("/")) url += "/";
        url += "api/states/";
        url += entityId.as<String>();
        
        HTTPClient http;
        http.begin(url);
        http.addHeader("Authorization", String("Bearer ") + haToken);
        http.setTimeout(3000);
        
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String payload = http.getString();
            JsonDocument personDoc;
            deserializeJson(personDoc, payload);
            
            // Extract person name
            String name;
            if (personDoc["attributes"]["friendly_name"]) {
                name = personDoc["attributes"]["friendly_name"].as<String>();
            } else {
                String entityIdStr = entityId.as<String>();
                name = entityIdStr.substring(entityIdStr.indexOf('.') + 1);
                if (name.length() > 0) name[0] = toupper(name[0]);
            }
            
            // Check if person is home
            bool present = (personDoc["state"].as<String>() == "home");
            
            lvglUI.updatePersonPresence(personIndex, name.c_str(), present, 0x00FF00);
            personIndex++;
        }
        
        http.end();
    }
}

void setTimezoneFromHA() {
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        log_e("Failed to load config for timezone");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    
    if (!haUrl || strlen(haUrl) == 0 || !haToken || strlen(haToken) == 0) {
        log_e("Home Assistant not configured for timezone");
        return;
    }
    
    // Fetch HA config to get timezone
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/config";
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + haToken);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        JsonDocument haConfig;
        DeserializationError error = deserializeJson(haConfig, payload);
        
        if (!error) {
            const char* timezone = haConfig["time_zone"];
            if (timezone && strlen(timezone) > 0) {
                // Convert timezone name to POSIX format
                // Most common European timezones
                String tz = String(timezone);
                String posixTz;
                
                if (tz == "Europe/Riga" || tz == "Europe/Helsinki" || tz == "Europe/Athens") {
                    posixTz = "EET-2EEST,M3.5.0/3,M10.5.0/4";
                } else if (tz == "Europe/London") {
                    posixTz = "GMT0BST,M3.5.0/1,M10.5.0";
                } else if (tz == "Europe/Berlin" || tz == "Europe/Paris" || tz == "Europe/Rome") {
                    posixTz = "CET-1CEST,M3.5.0,M10.5.0/3";
                } else if (tz == "America/New_York") {
                    posixTz = "EST5EDT,M3.2.0,M11.1.0";
                } else if (tz == "America/Los_Angeles") {
                    posixTz = "PST8PDT,M3.2.0,M11.1.0";
                } else {
                    // Default to UTC if unknown
                    posixTz = "UTC0";
                    log_w("Unknown timezone %s, using UTC", timezone);
                }
                
                setenv("TZ", posixTz.c_str(), 1);
                tzset();
                timezoneSet = true;
                log_i("Timezone set to: %s (POSIX: %s)", timezone, posixTz.c_str());
            } else {
                log_w("No timezone in HA config");
            }
        } else {
            log_e("Failed to parse HA config");
        }
    } else {
        log_e("Failed to fetch HA config: HTTP %d", httpCode);
    }
    
    http.end();
}

void updateCalendarDisplay() {
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        log_e("Failed to load config for calendar");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    const char* entityId = config["integrations"]["calendar"]["home_assistant"]["entity_id"] | "calendar.family";
    
    if (!haUrl || strlen(haUrl) == 0 || !haToken || strlen(haToken) == 0) {
        return;
    }
    
    // Build URL for calendar events
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/calendars/";
    url += entityId;
    
    // Get events for today only
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    char startDate[12];
    char endDate[12];
    
    // Start: today at 00:00
    snprintf(startDate, sizeof(startDate), "%04d-%02d-%02d",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
    
    // End: day after tomorrow at 00:00 (to include tomorrow's events)
    time_t endTime = now + (2 * 24 * 60 * 60);
    struct tm* endTimeinfo = localtime(&endTime);
    snprintf(endDate, sizeof(endDate), "%04d-%02d-%02d",
             endTimeinfo->tm_year + 1900, endTimeinfo->tm_mon + 1, endTimeinfo->tm_mday);
    
    url += "?start=";
    url += startDate;
    url += "&end=";
    url += endDate;
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + haToken);
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            JsonArray haEvents = doc.as<JsonArray>();
            int eventCount = min((int)haEvents.size(), MAX_CALENDAR_EVENTS);
            CalendarEvent events[MAX_CALENDAR_EVENTS];
            
            // Get today's and tomorrow's dates for comparison
            int todayDay = timeinfo->tm_mday;
            time_t tomorrow = now + (24 * 60 * 60);
            struct tm* tomorrowInfo = localtime(&tomorrow);
            int tomorrowDay = tomorrowInfo->tm_mday;
            
            for (int i = 0; i < eventCount; i++) {
                JsonObject event = haEvents[i];
                const char* summary = event["summary"] | "Event";
                const char* start = event["start"]["dateTime"] | event["start"]["date"];
                
                // Copy title
                strncpy(events[i].title, summary, sizeof(events[i].title) - 1);
                events[i].title[sizeof(events[i].title) - 1] = '\0';
                
                // Determine day and extract time
                if (start && strlen(start) >= 10) {
                    // Extract day from date (YYYY-MM-DD)
                    int eventDay = atoi(start + 8);
                    const char* dayLabel = "";
                    
                    if (eventDay == todayDay) {
                        dayLabel = "TODAY";
                    } else if (eventDay == tomorrowDay) {
                        dayLabel = "TOMORROW";
                    }
                    
                    // Check if it's a datetime or just date
                    if (strchr(start, 'T') && strlen(start) >= 16) {
                        // Has time component
                        snprintf(events[i].time, sizeof(events[i].time), "%s %.2s:%.2s", 
                                dayLabel, start + 11, start + 14);
                    } else {
                        // All-day event
                        snprintf(events[i].time, sizeof(events[i].time), "%s All day", dayLabel);
                    }
                } else {
                    strcpy(events[i].time, "");
                }
            }
            
            lvglUI.updateCalendar(events, eventCount);
        } else {
            log_e("Failed to parse calendar response: %s", error.c_str());
        }
    } else {
        log_e("Failed to fetch calendar from HA: HTTP %d", httpCode);
    }
    
    http.end();
}