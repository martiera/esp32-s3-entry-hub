#ifndef HA_ASSIST_CLIENT_H
#define HA_ASSIST_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Recording configuration
#define ASSIST_SAMPLE_RATE      16000
#define ASSIST_BITS_PER_SAMPLE  16
#define ASSIST_RECORD_SECONDS   5       // Max recording duration
#define ASSIST_AUDIO_BUFFER_SIZE (ASSIST_SAMPLE_RATE * ASSIST_RECORD_SECONDS * sizeof(int16_t))

// State machine for voice pipeline
enum AssistState {
    ASSIST_IDLE,
    ASSIST_RECORDING,
    ASSIST_PROCESSING_STT,
    ASSIST_PROCESSING_CONVERSATION,
    ASSIST_PROCESSING_TTS,
    ASSIST_ERROR
};

// Result callback
typedef void (*AssistResultCallback)(const char* transcription, const char* response, const char* error);

class HAAssistClient {
public:
    HAAssistClient();
    
    /**
     * Initialize the client with Home Assistant credentials
     * @param baseUrl HA base URL (e.g., "http://192.168.1.100:8123")
     * @param token Long-lived access token
     */
    void begin(const char* baseUrl, const char* token);
    
    /**
     * Process voice from audio buffer (main entry point)
     * Call this when voice activity is detected
     * 
     * @param audioBuffer 16-bit PCM audio samples
     * @param sampleCount Number of samples in buffer
     * @return true if processing started successfully
     */
    bool processVoice(const int16_t* audioBuffer, size_t sampleCount);
    
    /**
     * Start recording audio (call this when voice activity detected)
     * Audio will be collected via feedAudio() calls
     */
    void startRecording();
    
    /**
     * Feed audio samples during recording
     * @param samples Audio samples to add
     * @param count Number of samples
     */
    void feedAudio(const int16_t* samples, size_t count);
    
    /**
     * Stop recording and process the audio
     * @return true if processing started
     */
    bool stopAndProcess();
    
    /**
     * Send text directly to conversation API (skip STT)
     * Useful for touch/text input
     * 
     * @param text The text command to process
     * @return true if successful
     */
    bool sendTextCommand(const char* text);
    
    /**
     * Non-blocking loop - call regularly to check async operations
     */
    void loop();
    
    /**
     * Set callback for results
     */
    void setResultCallback(AssistResultCallback callback);
    
    // State getters
    AssistState getState() const { return _state; }
    bool isIdle() const { return _state == ASSIST_IDLE; }
    bool isBusy() const { return _state != ASSIST_IDLE && _state != ASSIST_ERROR; }
    const char* getLastTranscription() const { return _lastTranscription.c_str(); }
    const char* getLastResponse() const { return _lastResponse.c_str(); }
    const char* getLastError() const { return _lastError.c_str(); }
    
    // Pipeline configuration
    void setPipeline(const char* pipelineId);
    void setLanguage(const char* lang);
    void setSTTProvider(const char* provider);
    
private:
    String _baseUrl;
    String _token;
    String _pipelineId;
    String _language;
    String _sttProvider;  // STT provider name (e.g., "faster_whisper", "whisper", "cloud")
    
    AssistState _state;
    AssistResultCallback _callback;
    
    // Recording buffer (in PSRAM if available)
    int16_t* _recordBuffer;
    size_t _recordBufferSize;
    size_t _recordIndex;
    
    // Results
    String _lastTranscription;
    String _lastResponse;
    String _lastError;
    
    // Internal methods
    void discoverSTTProviders();
    bool sendToSTT(const int16_t* audioBuffer, size_t sampleCount);
    bool parseSTTResponse(const String& response);
    bool sendToConversation(const char* text);
    String makeRequest(const char* endpoint, const char* method, const char* contentType, 
                       const uint8_t* body, size_t bodyLen);
    String makeJsonRequest(const char* endpoint, JsonDocument& doc);
    
    // WAV encoding
    size_t createWavHeader(uint8_t* buffer, uint32_t dataSize);
};

extern HAAssistClient haAssist;

#endif
