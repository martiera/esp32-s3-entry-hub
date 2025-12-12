#include "led_feedback.h"
#include "pins.h"

// ESP32-S3 built-in neopixelWrite function for WS2812
// This is provided by the Arduino ESP32 core

LedFeedback ledFeedback;

LedFeedback::LedFeedback() 
    : ledPin(LED_PIN),
      maxBrightness(50),
      initialized(false),
      ledEnabled(true),
      currentPattern(LedPattern::OFF),
      currentColor(LedColor::Off()),
      patternSpeed(500),
      lastPatternUpdate(0),
      patternState(0),
      pulseValue(0),
      pulseDirection(true) {
}

bool LedFeedback::begin(uint8_t pin, uint8_t brightness) {
    ledPin = pin;
    maxBrightness = brightness;
    
    // ESP32-S3 DevKitC has WS2812 on GPIO48
    pinMode(ledPin, OUTPUT);
    
    // Turn off initially
    ::neopixelWrite(ledPin, 0, 0, 0);
    
    initialized = true;
    Serial.printf("LED Feedback initialized on GPIO %d, brightness %d%%\n", ledPin, (brightness * 100) / 255);
    
    return true;
}

void LedFeedback::loop() {
    if (!initialized || !ledEnabled) return;
    
    updatePattern();
}

void LedFeedback::setColor(LedColor color) {
    currentColor = color;
    currentPattern = LedPattern::SOLID;
    writeColor(color.r, color.g, color.b);
}

void LedFeedback::setColor(uint8_t r, uint8_t g, uint8_t b) {
    currentColor = {r, g, b};
    currentPattern = LedPattern::SOLID;
    writeColor(r, g, b);
}

void LedFeedback::setBrightness(uint8_t brightness) {
    maxBrightness = brightness;
}

void LedFeedback::off() {
    currentPattern = LedPattern::OFF;
    ::neopixelWrite(ledPin, 0, 0, 0);
}

void LedFeedback::setPattern(LedPattern pattern, LedColor color, uint16_t speed) {
    currentPattern = pattern;
    currentColor = color;
    patternSpeed = speed;
    patternState = 0;
    pulseValue = 0;
    pulseDirection = true;
    lastPatternUpdate = millis();
}

void LedFeedback::stopPattern() {
    currentPattern = LedPattern::OFF;
    off();
}

void LedFeedback::updatePattern() {
    unsigned long now = millis();
    
    switch (currentPattern) {
        case LedPattern::OFF:
            // Already off
            break;
            
        case LedPattern::SOLID:
            // Static color, no update needed
            break;
            
        case LedPattern::BLINK_SLOW:
        case LedPattern::BLINK_FAST: {
            uint16_t interval = (currentPattern == LedPattern::BLINK_SLOW) ? 500 : 100;
            if (now - lastPatternUpdate >= interval) {
                lastPatternUpdate = now;
                patternState = !patternState;
                if (patternState) {
                    writeColor(currentColor.r, currentColor.g, currentColor.b);
                } else {
                    ::neopixelWrite(ledPin, 0, 0, 0);
                }
            }
            break;
        }
            
        case LedPattern::PULSE: {
            if (now - lastPatternUpdate >= 20) {  // ~50Hz update
                lastPatternUpdate = now;
                
                if (pulseDirection) {
                    pulseValue += 5;
                    if (pulseValue >= 255) {
                        pulseValue = 255;
                        pulseDirection = false;
                    }
                } else {
                    pulseValue -= 5;
                    if (pulseValue <= 10) {
                        pulseValue = 10;
                        pulseDirection = true;
                    }
                }
                
                uint8_t scale = (pulseValue * maxBrightness) / 255;
                uint8_t r = (currentColor.r * scale) / 255;
                uint8_t g = (currentColor.g * scale) / 255;
                uint8_t b = (currentColor.b * scale) / 255;
                ::neopixelWrite(ledPin, r, g, b);
            }
            break;
        }
            
        case LedPattern::RAINBOW: {
            if (now - lastPatternUpdate >= 50) {
                lastPatternUpdate = now;
                patternState = (patternState + 1) % 256;
                
                // Rainbow color wheel
                uint8_t pos = patternState;
                uint8_t r, g, b;
                if (pos < 85) {
                    r = pos * 3;
                    g = 255 - pos * 3;
                    b = 0;
                } else if (pos < 170) {
                    pos -= 85;
                    r = 255 - pos * 3;
                    g = 0;
                    b = pos * 3;
                } else {
                    pos -= 170;
                    r = 0;
                    g = pos * 3;
                    b = 255 - pos * 3;
                }
                writeColor(r, g, b);
            }
            break;
        }
    }
}

void LedFeedback::writeColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!ledEnabled) {
        ::neopixelWrite(ledPin, 0, 0, 0);
        return;
    }
    
    // Apply brightness scaling
    r = (r * maxBrightness) / 255;
    g = (g * maxBrightness) / 255;
    b = (b * maxBrightness) / 255;
    
    ::neopixelWrite(ledPin, r, g, b);
}

// Preset feedback patterns
void LedFeedback::showBooting() {
    setPattern(LedPattern::PULSE, LedColor::Blue(), 20);
}

void LedFeedback::showWiFiConnecting() {
    setPattern(LedPattern::BLINK_SLOW, LedColor::Yellow(), 500);
}

void LedFeedback::showWiFiConnected() {
    setColor(LedColor::Green());
    // Flash green 3 times then turn off
}

void LedFeedback::showWakeWordDetected() {
    setColor(LedColor::Blue());
}

void LedFeedback::showListening() {
    setPattern(LedPattern::PULSE, LedColor::Cyan(), 20);
}

void LedFeedback::showProcessing() {
    setPattern(LedPattern::BLINK_FAST, LedColor::Purple(), 100);
}

void LedFeedback::showSuccess() {
    setColor(LedColor::Green());
}

void LedFeedback::showError() {
    setPattern(LedPattern::BLINK_FAST, LedColor::Red(), 100);
}

void LedFeedback::showIdle() {
    off();  // LED off when idle to save power
}

void LedFeedback::setEnabled(bool enabled) {
    ledEnabled = enabled;
    if (!enabled) {
        ::neopixelWrite(ledPin, 0, 0, 0);
    }
}
