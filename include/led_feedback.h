#ifndef LED_FEEDBACK_H
#define LED_FEEDBACK_H

#include <Arduino.h>

// LED Feedback patterns for user interaction
enum class LedPattern {
    OFF,
    SOLID,
    BLINK_SLOW,
    BLINK_FAST,
    PULSE,
    RAINBOW
};

// LED Colors (RGB values)
struct LedColor {
    uint8_t r, g, b;
    
    static LedColor Red()    { return {255, 0, 0}; }
    static LedColor Green()  { return {0, 255, 0}; }
    static LedColor Blue()   { return {0, 0, 255}; }
    static LedColor Yellow() { return {255, 255, 0}; }
    static LedColor Cyan()   { return {0, 255, 255}; }
    static LedColor Purple() { return {255, 0, 255}; }
    static LedColor White()  { return {255, 255, 255}; }
    static LedColor Orange() { return {255, 128, 0}; }
    static LedColor Off()    { return {0, 0, 0}; }
};

class LedFeedback {
public:
    LedFeedback();
    bool begin(uint8_t pin = 48, uint8_t brightness = 50);
    void loop();
    
    // Simple controls
    void setColor(LedColor color);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setBrightness(uint8_t brightness);
    void off();
    
    // Pattern controls
    void setPattern(LedPattern pattern, LedColor color, uint16_t speed = 500);
    void stopPattern();
    
    // Preset feedback patterns
    void showBooting();
    void showWiFiConnecting();
    void showWiFiConnected();
    void showWakeWordDetected();
    void showListening();
    void showProcessing();
    void showSuccess();
    void showError();
    void showIdle();
    
    // Enable/disable
    void setEnabled(bool enabled);
    bool isEnabled() const { return ledEnabled; }
    
private:
    uint8_t ledPin;
    uint8_t maxBrightness;
    bool initialized;
    bool ledEnabled;
    
    LedPattern currentPattern;
    LedColor currentColor;
    uint16_t patternSpeed;
    
    unsigned long lastPatternUpdate;
    uint8_t patternState;
    uint8_t pulseValue;
    bool pulseDirection;
    
    void updatePattern();
    void writeColor(uint8_t r, uint8_t g, uint8_t b);
};

extern LedFeedback ledFeedback;

#endif
