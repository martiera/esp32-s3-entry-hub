#include "audio_handler.h"
#include <Arduino.h>

AudioHandler audioHandler;

AudioHandler::AudioHandler() 
    : initialized(false), recording(false), bufferIndex(0) {
}

bool AudioHandler::begin() {
    log_i("Initializing I2S audio...");
    
    configureI2S();
    
    // Test I2S
    esp_err_t err = i2s_start(I2S_PORT);
    if (err != ESP_OK) {
        log_e("Failed to start I2S: %d", err);
        return false;
    }
    
    initialized = true;
    log_i("I2S audio initialized successfully");
    
    return true;
}

void AudioHandler::configureI2S() {
    log_i("Configuring I2S: SCK=%d WS=%d SD=%d", I2S_SCK_PIN, I2S_WS_PIN, I2S_SD_PIN);
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // INMP441 outputs 24-bit in 32-bit words
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // L/R floating = Left channel
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
        log_e("Failed to install I2S driver: %d", err);
        return;
    }
    
    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        log_e("Failed to set I2S pins: %d", err);
        return;
    }
    
    log_i("I2S driver installed successfully (32-bit samples, LEFT channel)");
    
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
    int32_t raw32[128];
    
    esp_err_t result = i2s_read(I2S_PORT, raw32, sizeof(raw32), &bytesRead, portMAX_DELAY);
    
    if (result == ESP_OK && bytesRead > 0) {
        size_t samplesRead = bytesRead / sizeof(int32_t);
        
        for (size_t i = 0; i < samplesRead && bufferIndex < AUDIO_BUFFER_SIZE; i++) {
            // Convert 32-bit to 16-bit
            audioBuffer[bufferIndex++] = (int16_t)(raw32[i] >> 16);
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
    
    // Read 32-bit samples from INMP441
    int32_t raw32[samples];
    size_t bytesRead = 0;
    // Use 50ms timeout to allow DMA buffer to fill with more samples
    // At 16kHz, 50ms = 800 samples, which should fill our 512 sample buffer
    esp_err_t result = i2s_read(I2S_PORT, raw32, samples * sizeof(int32_t), &bytesRead, 50);
    
    if (result == ESP_OK && bytesRead > 0) {
        size_t samplesRead = bytesRead / sizeof(int32_t);
        
        // Convert 32-bit samples to 16-bit by shifting down
        // INMP441 outputs 24-bit data in upper bits of 32-bit word
        for (size_t i = 0; i < samplesRead; i++) {
            buffer[i] = (int16_t)(raw32[i] >> 16);  // Take upper 16 bits of 24-bit data
        }
        
        return samplesRead;
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

bool AudioHandler::testMicrophone() {
    if (!initialized) {
        log_e("[MIC TEST] Audio not initialized!");
        return false;
    }
    
    log_i("[MIC TEST] Testing INMP441 microphone...");
    log_i("[MIC TEST] Pins: SCK=%d, WS=%d, SD=%d", I2S_SCK_PIN, I2S_WS_PIN, I2S_SD_PIN);
    log_i("[MIC TEST] Sample rate: %d Hz (32-bit I2S)", SAMPLE_RATE);
    
    // Read 32-bit samples - INMP441 outputs 24-bit left-justified
    int32_t raw32[256];
    int32_t totalSamples = 0;
    int32_t minVal = INT32_MAX;
    int32_t maxVal = INT32_MIN;
    int64_t sumAbs = 0;
    
    log_i("[MIC TEST] Reading 10 audio frames (speak NOW for best results)...");
    
    for (int frame = 0; frame < 10; frame++) {
        size_t bytesRead = 0;
        esp_err_t result = i2s_read(I2S_PORT, raw32, sizeof(raw32), &bytesRead, 100);
        
        if (result != ESP_OK) {
            log_e("[MIC TEST] I2S read error: %d", result);
            return false;
        }
        
        size_t samplesRead = bytesRead / sizeof(int32_t);
        totalSamples += samplesRead;
        
        for (size_t i = 0; i < samplesRead; i++) {
            int32_t val = raw32[i];  // Keep full 32-bit for analysis
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
            sumAbs += abs(val);
        }
        
        delay(10);
    }
    
    // Calculate average absolute value
    int32_t avgAbs = sumAbs / totalSamples;
    int32_t peakToPeak = maxVal - minVal;
    
    log_i("[MIC TEST] ===== RAW 32-bit Results =====");
    log_i("[MIC TEST] Samples read: %ld", totalSamples);
    log_i("[MIC TEST] Min value: %ld (0x%08lX)", minVal, (unsigned long)minVal);
    log_i("[MIC TEST] Max value: %ld (0x%08lX)", maxVal, (unsigned long)maxVal);
    log_i("[MIC TEST] Peak-to-peak: %ld", peakToPeak);
    log_i("[MIC TEST] Avg absolute: %ld", avgAbs);
    
    // Check for common issues
    if (minVal == 0 && maxVal == 0) {
        log_e("[MIC TEST] ALL ZEROS - No data! Check SD pin wiring to GPIO %d", I2S_SD_PIN);
        return false;
    }
    
    if (peakToPeak < 100000) {
        log_w("[MIC TEST] Very low signal!");
        log_w("[MIC TEST]    - Try connecting L/R pin to GND (not floating)");
        log_w("[MIC TEST]    - Or swap SCK/WS wires (pins 14<->15)");
        log_w("[MIC TEST]    - Speak loudly near the mic");
    } else if (peakToPeak > 100000000) {
        log_i("[MIC TEST] Good signal detected - microphone responding!");
    } else {
        log_i("[MIC TEST] Signal detected - try speaking louder");
    }
    
    log_i("[MIC TEST] =====================");
    return true;
}

void AudioHandler::monitorAudioLevels(int durationSeconds) {
    if (!initialized) {
        log_e("Audio not initialized!");
        return;
    }
    
    log_i("=== LIVE AUDIO MONITOR (32-bit) ===");
    log_i("Speak LOUDLY into the microphone - watch for level changes!");
    log_i("Running for %d seconds...", durationSeconds);
    
    int32_t samples[128];
    
    log_i("Starting monitor loop");
    
    for (int iter = 0; iter < 50; iter++) {
        size_t bytesRead = 0;
        esp_err_t result = i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, 100);
        
        if (result != ESP_OK) {
            log_w("Iter %d: i2s_read FAILED err=%d", iter, result);
            delay(200);
            continue;
        }
        
        if (bytesRead == 0) {
            log_w("Iter %d: bytesRead=0", iter);
            delay(200);
            continue;
        }
        
        size_t samplesRead = bytesRead / sizeof(int32_t);
        
        int32_t minVal = INT32_MAX;
        int32_t maxVal = INT32_MIN;
        
        for (size_t j = 0; j < samplesRead; j++) {
            if (samples[j] < minVal) minVal = samples[j];
            if (samples[j] > maxVal) maxVal = samples[j];
        }
        
        int64_t peakToPeak = (int64_t)maxVal - (int64_t)minVal;
        
        int barLen = (int)(peakToPeak / 100000000LL);
        if (barLen > 40) barLen = 40;
        if (barLen < 0) barLen = 0;
        
        char bar[42];
        for (int k = 0; k < barLen; k++) bar[k] = '#';
        for (int k = barLen; k < 40; k++) bar[k] = '-';
        bar[40] = '\0';
        
        // Use log_i for output
        if (peakToPeak > 500000000LL) {
            log_i("[%lld] |%s| LOUD!", peakToPeak, bar);
        } else if (peakToPeak > 200000000LL) {
            log_i("[%lld] |%s| Voice?", peakToPeak, bar);
        } else {
            log_i("[%lld] |%s|", peakToPeak, bar);
        }
        
        delay(200);
    }
    
    log_i("=== MONITOR ENDED ===");
}