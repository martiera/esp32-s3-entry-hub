#include "voice_activity_handler.h"
#include "config.h"

// Voice Activity Detection (VAD) implementation
// Detects voice/sound activity using audio level threshold

VoiceActivityHandler voiceActivity;

VoiceActivityHandler::VoiceActivityHandler() 
    : initialized(false), 
      voiceDetected(false), 
      sensitivity(WAKE_WORD_SENSITIVITY),
      wakeMode(WAKE_MODE_THRESHOLD),
      voiceThreshold(100000000),  // Lower threshold for easier triggering
      lastAudioLevel(0),
      lastDetectionTime(0),
      cooldownUntil(0) {
}

bool VoiceActivityHandler::begin() {
    log_i("Initializing Voice Activity Detection...");
    
    // Adjust threshold based on sensitivity
    // sensitivity 0.0 = hard to trigger (high threshold)
    // sensitivity 1.0 = easy to trigger (low threshold)
    // Lower base value for more sensitive detection
    voiceThreshold = (int32_t)(250000000LL * (1.0f - sensitivity * 0.9f));
    
    initialized = true;
    
    log_i("Voice Activity Detection initialized");
    log_i("Wake mode: %s", 
          wakeMode == WAKE_MODE_THRESHOLD ? "Audio Threshold" :
          wakeMode == WAKE_MODE_TOUCH ? "Touch Screen" :
          wakeMode == WAKE_MODE_BUTTON ? "Button" : "Disabled");
    log_i("Sensitivity: %.2f", sensitivity);
    log_i("Threshold: %ld", voiceThreshold);
    
    return true;
}

void VoiceActivityHandler::loop() {
    // Nothing to do in loop - processing happens in processAudioFrame
    // or via triggerWake() from touch/button handlers
}

bool VoiceActivityHandler::processAudioFrame(int16_t* frame, size_t frameLength) {
    if (!initialized || !frame || frameLength == 0) {
        return false;
    }
    
    // Only process if in threshold mode
    if (wakeMode != WAKE_MODE_THRESHOLD) {
        return false;
    }
    
    // Check cooldown
    if (millis() < cooldownUntil) {
        return false;
    }
    
    // Calculate peak-to-peak amplitude (energy measure)
    int16_t minVal = INT16_MAX;
    int16_t maxVal = INT16_MIN;
    int64_t sumAbs = 0;
    
    for (size_t i = 0; i < frameLength; i++) {
        int16_t sample = frame[i];
        if (sample < minVal) minVal = sample;
        if (sample > maxVal) maxVal = sample;
        sumAbs += abs(sample);
    }
    
    // Scale to approximate 32-bit range for comparison with threshold
    int32_t peakToPeak = ((int32_t)maxVal - (int32_t)minVal) * 65536;
    lastAudioLevel = peakToPeak;
    
    // Check if above threshold
    if (peakToPeak > voiceThreshold) {
        voiceDetected = true;
        lastDetectionTime = millis();
        cooldownUntil = millis() + COOLDOWN_MS;
        
        log_i("🎤 Voice activity detected! Level: %ld (threshold: %ld)", 
              peakToPeak, voiceThreshold);
        
        return true;
    }
    
    return false;
}

bool VoiceActivityHandler::isVoiceDetected() {
    return voiceDetected;
}

void VoiceActivityHandler::clearVoiceDetected() {
    voiceDetected = false;
}

void VoiceActivityHandler::setWakeMode(WakeMode mode) {
    wakeMode = mode;
    log_i("Wake mode set to: %s", 
          mode == WAKE_MODE_THRESHOLD ? "Audio Threshold" :
          mode == WAKE_MODE_TOUCH ? "Touch Screen" :
          mode == WAKE_MODE_BUTTON ? "Button" : "Disabled");
}

void VoiceActivityHandler::setThreshold(int32_t threshold) {
    voiceThreshold = threshold;
    log_i("Voice threshold set to: %ld", threshold);
}

void VoiceActivityHandler::setSensitivity(float newSensitivity) {
    sensitivity = constrain(newSensitivity, 0.0f, 1.0f);
    
    // Recalculate threshold based on new sensitivity
    voiceThreshold = (int32_t)(250000000LL * (1.0f - sensitivity * 0.9f));
    
    log_i("Sensitivity set to: %.2f (threshold: %ld)", sensitivity, voiceThreshold);
}

void VoiceActivityHandler::triggerWake() {
    // Check cooldown
    if (millis() < cooldownUntil) {
        log_d("Wake trigger ignored - cooldown active");
        return;
    }
    
    voiceDetected = true;
    lastDetectionTime = millis();
    cooldownUntil = millis() + COOLDOWN_MS;
    
    log_i("🎤 Manual wake triggered!");
}
