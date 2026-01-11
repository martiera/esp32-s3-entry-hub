#ifndef WYOMING_CLIENT_H
#define WYOMING_CLIENT_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// Wyoming protocol events
enum WyomingEventType {
    WYOMING_EVENT_AUDIO_START,
    WYOMING_EVENT_AUDIO_CHUNK,
    WYOMING_EVENT_AUDIO_STOP,
    WYOMING_EVENT_TRANSCRIPT,
    WYOMING_EVENT_ERROR,
    WYOMING_EVENT_UNKNOWN
};

// Wyoming client states
enum WyomingState {
    WYOMING_IDLE,
    WYOMING_CONNECTING,
    WYOMING_CONNECTED,
    WYOMING_STREAMING,
    WYOMING_PROCESSING,
    WYOMING_ERROR
};

// Callback types
typedef void (*WyomingTranscriptCallback)(const char* text);
typedef void (*WyomingErrorCallback)(const char* error);
typedef void (*WyomingStateCallback)(WyomingState state);

/**
 * Wyoming Protocol Client for Home Assistant Voice Streaming
 * 
 * Implements real-time audio streaming to Wyoming satellite for low-latency
 * speech-to-text processing. This replaces batch WAV upload with streaming
 * for 3-5x faster response times.
 * 
 * Wyoming Protocol Flow:
 * 1. Connect to Wyoming satellite TCP socket (default port 10300)
 * 2. Send AudioStart event with format info
 * 3. Stream AudioChunk events with PCM data (512 samples @ 16kHz)
 * 4. Send AudioStop when speech ends
 * 5. Receive Transcript event with recognized text
 * 
 * Benefits over REST API:
 * - STT processing starts immediately (no wait for full recording)
 * - Dynamic end-of-speech detection
 * - Lower memory usage (no full buffer needed)
 * - Real-time feedback from HA
 */
class WyomingClient {
public:
    WyomingClient();
    
    /**
     * Initialize Wyoming client
     * @param host Wyoming satellite host (IP or hostname)
     * @param port Wyoming satellite port (default 10300)
     */
    void begin(const char* host, uint16_t port = 10300);
    
    /**
     * Connect to Wyoming satellite and start streaming session
     * Must be called before streaming audio
     * @return true if connected successfully
     */
    bool connect();
    
    /**
     * Start audio streaming session
     * Sends AudioStart event with format info to Wyoming
     * @param sampleRate Audio sample rate (default 16000)
     * @param bitDepth Bits per sample (default 16)
     * @param channels Number of channels (default 1 - mono)
     * @return true if AudioStart sent successfully
     */
    bool startAudioStream(uint32_t sampleRate = 16000, uint16_t bitDepth = 16, uint8_t channels = 1);
    
    /**
     * Stream audio chunk to Wyoming satellite
     * Call this continuously with audio samples during recording
     * @param samples 16-bit PCM audio samples
     * @param count Number of samples (typically 512)
     * @return true if chunk sent successfully
     */
    bool streamAudioChunk(const int16_t* samples, size_t count);
    
    /**
     * Stop audio streaming and wait for transcript
     * Sends AudioStop event to signal end of speech
     * @return true if AudioStop sent successfully
     */
    bool stopAudioStream();
    
    /**
     * Disconnect from Wyoming satellite
     * Closes TCP connection
     */
    void disconnect();
    
    /**
     * Non-blocking loop - call regularly to process incoming messages
     * Handles Transcript and Error events from Wyoming
     */
    void loop();
    
    /**
     * Check if Wyoming is available and responding
     * Tests connection without starting audio stream
     * @return true if Wyoming satellite is reachable
     */
    bool isAvailable();
    
    // State getters
    WyomingState getState() const { return _state; }
    bool isConnected() const { return _state == WYOMING_CONNECTED || _state == WYOMING_STREAMING || _state == WYOMING_PROCESSING; }
    bool isStreaming() const { return _state == WYOMING_STREAMING; }
    bool isIdle() const { return _state == WYOMING_IDLE; }
    const char* getLastError() const { return _lastError.c_str(); }
    
    // Callbacks
    void setTranscriptCallback(WyomingTranscriptCallback callback) { _transcriptCallback = callback; }
    void setErrorCallback(WyomingErrorCallback callback) { _errorCallback = callback; }
    void setStateCallback(WyomingStateCallback callback) { _stateCallback = callback; }
    
    // Configuration
    void setTimeout(uint32_t timeoutMs) { _timeout = timeoutMs; }
    void setLanguage(const char* lang) { _language = lang; }
    
    // Statistics
    uint32_t getBytesStreamed() const { return _bytesStreamed; }
    uint32_t getChunksSent() const { return _chunksSent; }
    uint32_t getStreamDuration() const { return _streamDuration; }
    void resetStats();

private:
    // Connection info
    String _host;
    uint16_t _port;
    WiFiClient _client;
    
    // State
    WyomingState _state;
    String _lastError;
    uint32_t _timeout;
    String _language;
    
    // Streaming state
    unsigned long _streamStartTime;
    uint32_t _bytesStreamed;
    uint32_t _chunksSent;
    uint32_t _streamDuration;
    
    // Audio format
    uint32_t _sampleRate;
    uint16_t _bitDepth;
    uint8_t _channels;
    
    // Callbacks
    WyomingTranscriptCallback _transcriptCallback;
    WyomingErrorCallback _errorCallback;
    WyomingStateCallback _stateCallback;
    
    // Internal methods
    bool sendEvent(const char* eventType, JsonDocument& payload);
    bool sendAudioData(const uint8_t* data, size_t length);
    bool receiveEvent(String& eventType, JsonDocument& payload);
    void setState(WyomingState newState);
    void handleTranscript(JsonDocument& payload);
    void handleError(JsonDocument& payload);
    
    // Wyoming protocol helpers
    bool writeMessage(const String& message);
    bool readMessage(String& message);
};

extern WyomingClient wyomingClient;

#endif
