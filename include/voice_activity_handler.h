#ifndef VOICE_ACTIVITY_HANDLER_H
#define VOICE_ACTIVITY_HANDLER_H

#include <Arduino.h>

// Voice Activity Detection (VAD) Handler
// Detects voice/sound activity using audio level threshold
// 
// For actual wake word detection, consider:
// - Home Assistant Assist with Wyoming protocol
// - ESPHome voice_assistant component
// - microWakeWord (requires ESP-IDF, not Arduino)

enum WakeMode {
    WAKE_MODE_DISABLED = 0,     // No wake detection
    WAKE_MODE_THRESHOLD,        // Audio level threshold (voice activity)
    WAKE_MODE_TOUCH,            // Touch screen to wake
    WAKE_MODE_BUTTON            // Physical button to wake
};

class VoiceActivityHandler {
public:
    VoiceActivityHandler();
    
    bool begin();
    void loop();
    
    // Process audio frame - detects voice activity based on threshold
    // Returns true if voice activity detected
    bool processAudioFrame(int16_t* frame, size_t frameLength);
    
    // Wake detection status
    bool isVoiceDetected();
    void clearVoiceDetected();
    
    // Configuration
    void setWakeMode(WakeMode mode);
    WakeMode getWakeMode() const { return wakeMode; }
    
    void setThreshold(int32_t threshold);
    int32_t getThreshold() const { return voiceThreshold; }
    
    void setSensitivity(float sensitivity);
    float getSensitivity() const { return sensitivity; }
    
    // For manual triggering (touch/button)
    void triggerWake();
    
    bool isInitialized() const { return initialized; }
    
    // Statistics
    int32_t getLastAudioLevel() const { return lastAudioLevel; }
    unsigned long getLastDetectionTime() const { return lastDetectionTime; }
    
private:
    bool initialized;
    bool voiceDetected;
    float sensitivity;
    
    WakeMode wakeMode;
    int32_t voiceThreshold;      // Threshold for voice activity detection
    int32_t lastAudioLevel;      // Last measured audio level
    unsigned long lastDetectionTime;
    unsigned long cooldownUntil; // Prevents rapid re-triggering
    
    static const unsigned long COOLDOWN_MS = 2000; // 2 second cooldown after detection
};

extern VoiceActivityHandler voiceActivity;

#endif
