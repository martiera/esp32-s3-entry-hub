#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <driver/i2s.h>
#include "config.h"

class AudioHandler {
public:
    AudioHandler();
    bool begin();
    void loop();
    
    size_t readAudio(int16_t* buffer, size_t samples);
    bool isRecording();
    void startRecording();
    void stopRecording();
    
    int16_t* getAudioBuffer();
    size_t getBufferSize();
    
private:
    bool initialized;
    bool recording;
    int16_t audioBuffer[AUDIO_BUFFER_SIZE];
    size_t bufferIndex;
    
    void configureI2S();
    void processAudio();
};

extern AudioHandler audioHandler;

#endif
