#include "porcupine_handler.h"
#include "config.h"

// IMPORTANT: This is a template implementation
// You need to download Porcupine Arduino SDK and integrate it
// Download from: https://github.com/Picovoice/porcupine/tree/master/binding/arduino

PorcupineHandler porcupineHandler;

PorcupineHandler::PorcupineHandler() 
    : initialized(false), 
      wakeWordDetected(false), 
      detectedIndex(-1),
      sensitivity(WAKE_WORD_SENSITIVITY),
      porcupineContext(nullptr) {
    strcpy(currentWakeWord, "jarvis");
}

bool PorcupineHandler::begin() {
    Serial.println("Initializing Porcupine wake word detection...");
    
    // TODO: Initialize actual Porcupine library
    // This requires:
    // 1. Download Porcupine Arduino library
    // 2. Place .ppn keyword files in data/keywords/
    // 3. Initialize with access key from secrets.h
    
    /*
    Example initialization (pseudo-code):
    
    #include "pv_porcupine.h"
    
    pv_status_t status = pv_porcupine_init(
        PORCUPINE_ACCESS_KEY,
        1,                          // num keywords
        keyword_paths,              // paths to .ppn files
        &sensitivity,
        &porcupineContext
    );
    
    if (status != PV_STATUS_SUCCESS) {
        Serial.println("Porcupine init failed");
        return false;
    }
    */
    
    initialized = true;
    Serial.println("Porcupine initialized (template mode)");
    Serial.println("⚠️  INTEGRATION REQUIRED: Add Porcupine Arduino SDK to lib/");
    
    return true;
}

void PorcupineHandler::loop() {
    if (!initialized) {
        return;
    }
    
    // Reset detection flag
    if (wakeWordDetected) {
        wakeWordDetected = false;
        detectedIndex = -1;
    }
}

bool PorcupineHandler::processAudioFrame(int16_t* frame, size_t frameLength) {
    if (!initialized || !frame) {
        return false;
    }
    
    // TODO: Process frame with actual Porcupine library
    /*
    Example processing (pseudo-code):
    
    int32_t keyword_index;
    pv_status_t status = pv_porcupine_process(
        porcupineContext,
        frame,
        &keyword_index
    );
    
    if (status != PV_STATUS_SUCCESS) {
        return false;
    }
    
    if (keyword_index >= 0) {
        wakeWordDetected = true;
        detectedIndex = keyword_index;
        Serial.printf("Wake word detected: %s\n", currentWakeWord);
        return true;
    }
    */
    
    // Simulated detection for testing (remove when integrating real library)
    static int frameCounter = 0;
    frameCounter++;
    
    // Simulate detection every ~5 seconds at 16kHz
    if (frameCounter > 80000) {
        frameCounter = 0;
        wakeWordDetected = true;
        detectedIndex = 0;
        Serial.println("⚠️  Simulated wake word detection (replace with real Porcupine)");
        return true;
    }
    
    return false;
}

bool PorcupineHandler::isWakeWordDetected() {
    return wakeWordDetected;
}

int PorcupineHandler::getDetectedKeywordIndex() {
    return detectedIndex;
}

const char* PorcupineHandler::getDetectedKeyword() {
    return currentWakeWord;
}

void PorcupineHandler::setWakeWord(const char* wakeWord) {
    if (wakeWord && strlen(wakeWord) < sizeof(currentWakeWord)) {
        strcpy(currentWakeWord, wakeWord);
        Serial.printf("Wake word set to: %s\n", currentWakeWord);
        
        // TODO: Reinitialize Porcupine with new keyword
        // This requires loading the corresponding .ppn file
    }
}

void PorcupineHandler::setSensitivity(float newSensitivity) {
    sensitivity = constrain(newSensitivity, 0.0f, 1.0f);
    Serial.printf("Sensitivity set to: %.2f\n", sensitivity);
    
    // TODO: Update Porcupine sensitivity
}

bool PorcupineHandler::initializePorcupine() {
    // TODO: Actual initialization code
    return true;
}

void PorcupineHandler::cleanupPorcupine() {
    if (porcupineContext) {
        // TODO: pv_porcupine_delete(porcupineContext);
        porcupineContext = nullptr;
    }
}
