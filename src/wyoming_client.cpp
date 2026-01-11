#include "wyoming_client.h"
#include <WiFi.h>
#include <base64.h>

WyomingClient wyomingClient;

WyomingClient::WyomingClient()
    : _port(10300)
    , _state(WYOMING_IDLE)
    , _timeout(30000)
    , _language("en")
    , _streamStartTime(0)
    , _bytesStreamed(0)
    , _chunksSent(0)
    , _streamDuration(0)
    , _sampleRate(16000)
    , _bitDepth(16)
    , _channels(1)
    , _transcriptCallback(nullptr)
    , _errorCallback(nullptr)
    , _stateCallback(nullptr)
{
}

void WyomingClient::begin(const char* host, uint16_t port) {
    _host = host;
    _port = port;
    
    log_i("Wyoming: Initialized with host=%s port=%d", _host.c_str(), _port);
}

bool WyomingClient::connect() {
    if (_state == WYOMING_CONNECTED || _state == WYOMING_STREAMING) {
        log_w("Wyoming: Already connected");
        return true;
    }
    
    setState(WYOMING_CONNECTING);
    
    log_i("Wyoming: Connecting to %s:%d...", _host.c_str(), _port);
    
    if (!_client.connect(_host.c_str(), _port)) {
        _lastError = "Failed to connect to Wyoming satellite";
        log_e("Wyoming: %s", _lastError.c_str());
        setState(WYOMING_ERROR);
        return false;
    }
    
    log_i("Wyoming: Connected successfully");
    setState(WYOMING_CONNECTED);
    return true;
}

bool WyomingClient::startAudioStream(uint32_t sampleRate, uint16_t bitDepth, uint8_t channels) {
    if (_state != WYOMING_CONNECTED) {
        _lastError = "Not connected to Wyoming satellite";
        log_e("Wyoming: %s", _lastError.c_str());
        return false;
    }
    
    _sampleRate = sampleRate;
    _bitDepth = bitDepth;
    _channels = channels;
    _streamStartTime = millis();
    _bytesStreamed = 0;
    _chunksSent = 0;
    
    // Create AudioStart event
    JsonDocument doc;
    doc["type"] = "audio-start";
    
    JsonObject audioStart = doc.createNestedObject("data");
    audioStart["rate"] = sampleRate;
    audioStart["width"] = bitDepth / 8;  // bytes per sample
    audioStart["channels"] = channels;
    
    log_i("Wyoming: Starting audio stream (rate=%d, width=%d, channels=%d)", 
          sampleRate, bitDepth / 8, channels);
    
    if (!sendEvent("audio-start", doc)) {
        _lastError = "Failed to send AudioStart event";
        log_e("Wyoming: %s", _lastError.c_str());
        setState(WYOMING_ERROR);
        return false;
    }
    
    setState(WYOMING_STREAMING);
    log_i("Wyoming: Audio stream started");
    return true;
}

bool WyomingClient::streamAudioChunk(const int16_t* samples, size_t count) {
    if (_state != WYOMING_STREAMING) {
        log_w("Wyoming: Not in streaming state (state=%d)", _state);
        return false;
    }
    
    if (!_client.connected()) {
        _lastError = "Connection lost during streaming";
        log_e("Wyoming: %s", _lastError.c_str());
        setState(WYOMING_ERROR);
        return false;
    }
    
    // Convert samples to bytes (little-endian 16-bit PCM)
    size_t dataSize = count * sizeof(int16_t);
    uint8_t* audioData = (uint8_t*)samples;
    
    // Create AudioChunk event
    JsonDocument doc;
    doc["type"] = "audio-chunk";
    
    JsonObject audioChunk = doc.createNestedObject("data");
    audioChunk["rate"] = _sampleRate;
    audioChunk["width"] = _bitDepth / 8;
    audioChunk["channels"] = _channels;
    
    // Encode audio as base64
    String base64Audio = base64::encode(audioData, dataSize);
    audioChunk["audio"] = base64Audio;
    
    if (!sendEvent("audio-chunk", doc)) {
        log_w("Wyoming: Failed to send audio chunk");
        return false;
    }
    
    _bytesStreamed += dataSize;
    _chunksSent++;
    
    // Log progress every 50 chunks (~1.6 seconds @ 512 samples/chunk)
    if (_chunksSent % 50 == 0) {
        _streamDuration = millis() - _streamStartTime;
        log_d("Wyoming: Streamed %d chunks, %d bytes, %.2f seconds", 
              _chunksSent, _bytesStreamed, _streamDuration / 1000.0f);
    }
    
    return true;
}

bool WyomingClient::stopAudioStream() {
    if (_state != WYOMING_STREAMING) {
        log_w("Wyoming: Not streaming (state=%d)", _state);
        return false;
    }
    
    _streamDuration = millis() - _streamStartTime;
    
    log_i("Wyoming: Stopping audio stream (duration=%.2fs, chunks=%d, bytes=%d)", 
          _streamDuration / 1000.0f, _chunksSent, _bytesStreamed);
    
    // Create AudioStop event
    JsonDocument doc;
    doc["type"] = "audio-stop";
    
    if (!sendEvent("audio-stop", doc)) {
        _lastError = "Failed to send AudioStop event";
        log_e("Wyoming: %s", _lastError.c_str());
        setState(WYOMING_ERROR);
        return false;
    }
    
    setState(WYOMING_PROCESSING);
    log_i("Wyoming: Audio stream stopped, waiting for transcript...");
    return true;
}

void WyomingClient::disconnect() {
    if (_client.connected()) {
        _client.stop();
        log_i("Wyoming: Disconnected");
    }
    setState(WYOMING_IDLE);
}

void WyomingClient::loop() {
    // Check for incoming messages when connected
    if (_state == WYOMING_PROCESSING && _client.connected() && _client.available()) {
        String eventType;
        JsonDocument payload;
        
        if (receiveEvent(eventType, payload)) {
            if (eventType == "transcript") {
                handleTranscript(payload);
            } else if (eventType == "error") {
                handleError(payload);
            } else {
                log_d("Wyoming: Received unknown event: %s", eventType.c_str());
            }
        }
    }
    
    // Check for connection timeout
    if (_state == WYOMING_PROCESSING) {
        unsigned long elapsed = millis() - _streamStartTime;
        if (elapsed > _timeout) {
            _lastError = "Transcript timeout";
            log_e("Wyoming: %s after %dms", _lastError.c_str(), elapsed);
            setState(WYOMING_ERROR);
            if (_errorCallback) {
                _errorCallback(_lastError.c_str());
            }
            disconnect();
        }
    }
}

bool WyomingClient::isAvailable() {
    log_i("Wyoming: Testing availability at %s:%d...", _host.c_str(), _port);
    
    WiFiClient testClient;
    if (!testClient.connect(_host.c_str(), _port, 5000)) {
        log_w("Wyoming: Not available (connection failed)");
        return false;
    }
    
    testClient.stop();
    log_i("Wyoming: Available");
    return true;
}

void WyomingClient::resetStats() {
    _bytesStreamed = 0;
    _chunksSent = 0;
    _streamDuration = 0;
}

bool WyomingClient::sendEvent(const char* eventType, JsonDocument& payload) {
    if (!_client.connected()) {
        log_e("Wyoming: Cannot send event, not connected");
        return false;
    }
    
    // Serialize to JSON string
    String message;
    serializeJson(payload, message);
    
    log_d("Wyoming: Sending %s: %s", eventType, message.c_str());
    
    return writeMessage(message);
}

bool WyomingClient::receiveEvent(String& eventType, JsonDocument& payload) {
    String message;
    if (!readMessage(message)) {
        return false;
    }
    
    log_d("Wyoming: Received message: %s", message.c_str());
    
    // Parse JSON
    DeserializationError error = deserializeJson(payload, message);
    if (error) {
        log_e("Wyoming: Failed to parse message: %s", error.c_str());
        return false;
    }
    
    // Extract event type
    if (payload.containsKey("type")) {
        eventType = payload["type"].as<String>();
        return true;
    }
    
    log_e("Wyoming: Message missing 'type' field");
    return false;
}

void WyomingClient::setState(WyomingState newState) {
    if (_state != newState) {
        log_d("Wyoming: State change: %d -> %d", _state, newState);
        _state = newState;
        
        if (_stateCallback) {
            _stateCallback(newState);
        }
    }
}

void WyomingClient::handleTranscript(JsonDocument& payload) {
    log_i("Wyoming: Received transcript event");
    
    // Extract transcript text from payload
    // Expected format: {"type": "transcript", "data": {"text": "..."}}
    String text;
    if (payload.containsKey("data")) {
        JsonObject data = payload["data"];
        if (data.containsKey("text")) {
            text = data["text"].as<String>();
        }
    }
    
    if (text.length() == 0) {
        log_w("Wyoming: Empty transcript received");
        text = "";  // Empty transcript is valid (silence)
    }
    
    log_i("Wyoming: Transcript: '%s'", text.c_str());
    
    // Call transcript callback
    if (_transcriptCallback) {
        _transcriptCallback(text.c_str());
    }
    
    // Disconnect after receiving transcript
    disconnect();
}

void WyomingClient::handleError(JsonDocument& payload) {
    log_e("Wyoming: Received error event");
    
    // Extract error message
    String errorMsg = "Unknown error";
    if (payload.containsKey("data")) {
        JsonObject data = payload["data"];
        if (data.containsKey("message")) {
            errorMsg = data["message"].as<String>();
        } else if (data.containsKey("error")) {
            errorMsg = data["error"].as<String>();
        }
    }
    
    _lastError = errorMsg;
    log_e("Wyoming: Error: %s", errorMsg.c_str());
    
    setState(WYOMING_ERROR);
    
    // Call error callback
    if (_errorCallback) {
        _errorCallback(errorMsg.c_str());
    }
    
    disconnect();
}

bool WyomingClient::writeMessage(const String& message) {
    if (!_client.connected()) {
        return false;
    }
    
    // Wyoming protocol: newline-delimited JSON messages
    String fullMessage = message + "\n";
    
    size_t written = _client.print(fullMessage);
    if (written != fullMessage.length()) {
        log_e("Wyoming: Write failed (wrote %d/%d bytes)", written, fullMessage.length());
        return false;
    }
    
    _client.flush();
    return true;
}

bool WyomingClient::readMessage(String& message) {
    if (!_client.connected() || !_client.available()) {
        return false;
    }
    
    // Wyoming protocol: newline-delimited JSON messages
    message = _client.readStringUntil('\n');
    message.trim();
    
    if (message.length() == 0) {
        return false;
    }
    
    return true;
}
