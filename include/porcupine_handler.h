#ifndef PORCUPINE_HANDLER_H
#define PORCUPINE_HANDLER_H

#include <Arduino.h>

// Note: Porcupine library files need to be placed in lib/porcupine/
// Download from: https://github.com/Picovoice/porcupine
// This is a wrapper interface for the Porcupine wake word engine

class PorcupineHandler {
public:
    PorcupineHandler();
    bool begin();
    void loop();
    
    bool processAudioFrame(int16_t* frame, size_t frameLength);
    bool isWakeWordDetected();
    int getDetectedKeywordIndex();
    const char* getDetectedKeyword();
    
    void setWakeWord(const char* wakeWord);
    void setSensitivity(float sensitivity);
    
private:
    bool initialized;
    bool wakeWordDetected;
    int detectedIndex;
    float sensitivity;
    char currentWakeWord[32];
    
    // Porcupine context (will be initialized with actual library)
    void* porcupineContext;
    
    bool initializePorcupine();
    void cleanupPorcupine();
};

extern PorcupineHandler porcupineHandler;

#endif
