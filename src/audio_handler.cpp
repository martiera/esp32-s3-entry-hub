#include "audio_handler.h"
#include <Arduino.h>

AudioHandler audioHandler;

AudioHandler::AudioHandler() 
    : initialized(false), recording(false), bufferIndex(0) {
}

bool AudioHandler::begin() {
    Serial.println("Initializing I2S audio...");
    
    configureI2S();
    
    // Test I2S
    i2s_start(I2S_PORT);
    
    initialized = true;
    Serial.println("I2S audio initialized successfully");
    
    return true;
}

void AudioHandler::configureI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };
    
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %d\n", err);
        return;
    }
    
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("Failed to set I2S pins: %d\n", err);
        return;
    }
    
    // Clear DMA buffers
    i2s_zero_dma_buffer(I2S_PORT);
}

void AudioHandler::loop() {
    if (!initialized || !recording) {
        return;
    }
    
    processAudio();
}

void AudioHandler::processAudio() {
    size_t bytesRead = 0;
    int16_t samples[128];
    
    esp_err_t result = i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    
    if (result == ESP_OK && bytesRead > 0) {
        size_t samplesRead = bytesRead / sizeof(int16_t);
        
        for (size_t i = 0; i < samplesRead && bufferIndex < AUDIO_BUFFER_SIZE; i++) {
            audioBuffer[bufferIndex++] = samples[i];
        }
        
        // Buffer full - reset for continuous recording
        if (bufferIndex >= AUDIO_BUFFER_SIZE) {
            bufferIndex = 0;
        }
    }
}

size_t AudioHandler::readAudio(int16_t* buffer, size_t samples) {
    if (!initialized) {
        return 0;
    }
    
    size_t bytesRead = 0;
    esp_err_t result = i2s_read(I2S_PORT, buffer, samples * sizeof(int16_t), &bytesRead, 100);
    
    if (result == ESP_OK) {
        return bytesRead / sizeof(int16_t);
    }
    
    return 0;
}

bool AudioHandler::isRecording() {
    return recording;
}

void AudioHandler::startRecording() {
    if (!initialized) {
        return;
    }
    
    recording = true;
    bufferIndex = 0;
    i2s_zero_dma_buffer(I2S_PORT);
    Serial.println("Audio recording started");
}

void AudioHandler::stopRecording() {
    recording = false;
    Serial.println("Audio recording stopped");
}

int16_t* AudioHandler::getAudioBuffer() {
    return audioBuffer;
}

size_t AudioHandler::getBufferSize() {
    return bufferIndex;
}
