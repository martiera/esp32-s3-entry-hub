#include "ha_assist_client.h"
#include <WiFi.h>

HAAssistClient haAssist;

HAAssistClient::HAAssistClient() 
    : _state(ASSIST_IDLE)
    , _callback(nullptr)
    , _recordBuffer(nullptr)
    , _recordBufferSize(0)
    , _recordIndex(0)
    , _language("en")
{
}

void HAAssistClient::begin(const char* baseUrl, const char* token) {
    _baseUrl = baseUrl;
    _token = token;
    
    // Remove trailing slash if present
    if (_baseUrl.endsWith("/")) {
        _baseUrl.remove(_baseUrl.length() - 1);
    }
    
    // Allocate recording buffer in PSRAM if available
    _recordBufferSize = ASSIST_AUDIO_BUFFER_SIZE / sizeof(int16_t);
    
    if (psramFound()) {
        _recordBuffer = (int16_t*)ps_malloc(ASSIST_AUDIO_BUFFER_SIZE);
        log_i("HAAssist: Allocated %d bytes in PSRAM for audio buffer", ASSIST_AUDIO_BUFFER_SIZE);
    } else {
        // Fallback to smaller buffer in regular RAM
        _recordBufferSize = 16000 * 2; // 2 seconds max
        _recordBuffer = (int16_t*)malloc(_recordBufferSize * sizeof(int16_t));
        log_w("HAAssist: No PSRAM, using smaller buffer (%d samples)", _recordBufferSize);
    }
    
    if (!_recordBuffer) {
        log_e("HAAssist: Failed to allocate audio buffer!");
        return;
    }
    
    log_i("HAAssist: Initialized with URL: %s", _baseUrl.c_str());
    
    // Try to discover STT providers
    discoverSTTProviders();
}

void HAAssistClient::discoverSTTProviders() {
    if (!WiFi.isConnected()) {
        log_w("HAAssist: WiFi not connected, skipping STT discovery");
        return;
    }
    
    HTTPClient http;
    String url = _baseUrl + "/api/states";
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + _token);
    http.setTimeout(10000);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        
        // Look for stt.* entities
        int idx = 0;
        while ((idx = response.indexOf("\"entity_id\":\"stt.", idx)) != -1) {
            int start = idx + 17; // After "entity_id":"stt.
            int end = response.indexOf("\"", start);
            if (end > start) {
                String provider = response.substring(start, end);
                if (_sttProvider.length() == 0) {
                    _sttProvider = provider;
                    log_i("HAAssist: Found STT provider: stt.%s (using as default)", provider.c_str());
                } else {
                    log_i("HAAssist: Found STT provider: stt.%s", provider.c_str());
                }
            }
            idx = end;
        }
        
        if (_sttProvider.length() == 0) {
            log_w("HAAssist: No STT providers found in Home Assistant!");
            log_w("HAAssist: Please set up Whisper or another STT provider in HA.");
        }
    } else {
        log_e("HAAssist: Failed to query HA states: HTTP %d", httpCode);
    }
    
    http.end();
}

void HAAssistClient::setResultCallback(AssistResultCallback callback) {
    _callback = callback;
}

void HAAssistClient::setPipeline(const char* pipelineId) {
    _pipelineId = pipelineId;
}

void HAAssistClient::setLanguage(const char* lang) {
    _language = lang;
}

void HAAssistClient::setSTTProvider(const char* provider) {
    _sttProvider = provider;
    log_i("HAAssist: STT provider set to: %s", provider);
}

void HAAssistClient::startRecording() {
    if (_state != ASSIST_IDLE) {
        log_w("HAAssist: Cannot start recording, state=%d", _state);
        return;
    }
    
    _recordIndex = 0;
    _state = ASSIST_RECORDING;
    log_i("HAAssist: Started recording...");
}

void HAAssistClient::feedAudio(const int16_t* samples, size_t count) {
    if (_state != ASSIST_RECORDING || !_recordBuffer) {
        return;
    }
    
    // Copy samples to buffer
    size_t remaining = _recordBufferSize - _recordIndex;
    size_t toCopy = min(count, remaining);
    
    if (toCopy > 0) {
        memcpy(&_recordBuffer[_recordIndex], samples, toCopy * sizeof(int16_t));
        _recordIndex += toCopy;
    }
    
    // Check if buffer is full
    if (_recordIndex >= _recordBufferSize) {
        log_w("HAAssist: Recording buffer full, stopping");
        stopAndProcess();
    }
}

bool HAAssistClient::stopAndProcess() {
    if (_state != ASSIST_RECORDING) {
        log_w("HAAssist: Not recording, cannot stop");
        return false;
    }
    
    log_i("HAAssist: Stopping recording, got %d samples (%.2f seconds)", 
          _recordIndex, (float)_recordIndex / ASSIST_SAMPLE_RATE);
    
    if (_recordIndex < ASSIST_SAMPLE_RATE / 4) { // Less than 0.25 seconds
        log_w("HAAssist: Recording too short, ignoring");
        _state = ASSIST_IDLE;
        return false;
    }
    
    return processVoice(_recordBuffer, _recordIndex);
}

bool HAAssistClient::processVoice(const int16_t* audioBuffer, size_t sampleCount) {
    if (!WiFi.isConnected()) {
        _lastError = "WiFi not connected";
        _state = ASSIST_ERROR;
        if (_callback) _callback(nullptr, nullptr, _lastError.c_str());
        return false;
    }
    
    _state = ASSIST_PROCESSING_STT;
    log_i("HAAssist: Processing %d audio samples...", sampleCount);
    
    // Send to STT only - skip conversation API
    if (!sendToSTT(audioBuffer, sampleCount)) {
        _state = ASSIST_ERROR;
        if (_callback) _callback(nullptr, nullptr, _lastError.c_str());
        _state = ASSIST_IDLE;
        return false;
    }
    
    // Success - return transcription for local processing
    log_i("HAAssist: STT complete! Transcription: '%s'", _lastTranscription.c_str());
    
    if (_callback) {
        // Pass transcription, no response from conversation API
        _callback(_lastTranscription.c_str(), nullptr, nullptr);
    }
    
    _state = ASSIST_IDLE;
    return true;
}

bool HAAssistClient::sendTextCommand(const char* text) {
    if (!WiFi.isConnected()) {
        _lastError = "WiFi not connected";
        return false;
    }
    
    _lastTranscription = text;
    _state = ASSIST_PROCESSING_CONVERSATION;
    
    if (!sendToConversation(text)) {
        _state = ASSIST_IDLE;
        return false;
    }
    
    if (_callback) {
        _callback(text, _lastResponse.c_str(), nullptr);
    }
    
    _state = ASSIST_IDLE;
    return true;
}

size_t HAAssistClient::createWavHeader(uint8_t* buffer, uint32_t dataSize) {
    // WAV header structure
    uint32_t fileSize = dataSize + 36;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 1; // Mono
    uint32_t sampleRate = ASSIST_SAMPLE_RATE;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;
    
    size_t i = 0;
    
    // RIFF header
    buffer[i++] = 'R'; buffer[i++] = 'I'; buffer[i++] = 'F'; buffer[i++] = 'F';
    buffer[i++] = fileSize & 0xFF;
    buffer[i++] = (fileSize >> 8) & 0xFF;
    buffer[i++] = (fileSize >> 16) & 0xFF;
    buffer[i++] = (fileSize >> 24) & 0xFF;
    buffer[i++] = 'W'; buffer[i++] = 'A'; buffer[i++] = 'V'; buffer[i++] = 'E';
    
    // fmt chunk
    buffer[i++] = 'f'; buffer[i++] = 'm'; buffer[i++] = 't'; buffer[i++] = ' ';
    buffer[i++] = 16; buffer[i++] = 0; buffer[i++] = 0; buffer[i++] = 0; // Chunk size
    buffer[i++] = audioFormat & 0xFF; buffer[i++] = (audioFormat >> 8) & 0xFF;
    buffer[i++] = numChannels & 0xFF; buffer[i++] = (numChannels >> 8) & 0xFF;
    buffer[i++] = sampleRate & 0xFF;
    buffer[i++] = (sampleRate >> 8) & 0xFF;
    buffer[i++] = (sampleRate >> 16) & 0xFF;
    buffer[i++] = (sampleRate >> 24) & 0xFF;
    buffer[i++] = byteRate & 0xFF;
    buffer[i++] = (byteRate >> 8) & 0xFF;
    buffer[i++] = (byteRate >> 16) & 0xFF;
    buffer[i++] = (byteRate >> 24) & 0xFF;
    buffer[i++] = blockAlign & 0xFF; buffer[i++] = (blockAlign >> 8) & 0xFF;
    buffer[i++] = bitsPerSample & 0xFF; buffer[i++] = (bitsPerSample >> 8) & 0xFF;
    
    // data chunk header
    buffer[i++] = 'd'; buffer[i++] = 'a'; buffer[i++] = 't'; buffer[i++] = 'a';
    buffer[i++] = dataSize & 0xFF;
    buffer[i++] = (dataSize >> 8) & 0xFF;
    buffer[i++] = (dataSize >> 16) & 0xFF;
    buffer[i++] = (dataSize >> 24) & 0xFF;
    
    return i; // Should be 44
}

bool HAAssistClient::sendToSTT(const int16_t* audioBuffer, size_t sampleCount) {
    log_i("HAAssist: Sending %d samples to STT...", sampleCount);
    
    // Calculate sizes
    uint32_t audioDataSize = sampleCount * sizeof(int16_t);
    uint32_t wavSize = 44 + audioDataSize;
    
    // Allocate WAV buffer
    uint8_t* wavBuffer = nullptr;
    if (psramFound()) {
        wavBuffer = (uint8_t*)ps_malloc(wavSize);
    } else {
        wavBuffer = (uint8_t*)malloc(wavSize);
    }
    
    if (!wavBuffer) {
        _lastError = "Failed to allocate WAV buffer";
        log_e("HAAssist: %s", _lastError.c_str());
        return false;
    }
    
    // Create WAV header
    createWavHeader(wavBuffer, audioDataSize);
    
    // Copy audio data
    memcpy(wavBuffer + 44, audioBuffer, audioDataSize);
    
    log_i("HAAssist: Created WAV file: %d bytes", wavSize);
    
    // Try different STT API endpoints
    // HA STT API: POST /api/stt/stt.{provider_name}
    
    HTTPClient http;
    
    // Try to get list of STT providers first if we don't have one configured
    if (_sttProvider.length() == 0) {
        // Try common provider names (entity format is stt.{name})
        const char* providers[] = {"faster_whisper", "whisper", "cloud", "google_translate", "vosk"};
        
        for (int i = 0; i < 5; i++) {
            String testUrl = _baseUrl + "/api/stt/stt." + providers[i];
            log_i("HAAssist: Trying STT provider: stt.%s", providers[i]);
            
            http.begin(testUrl);
            http.addHeader("Authorization", String("Bearer ") + _token);
            http.addHeader("Content-Type", "audio/wav");
            http.addHeader("X-Speech-Content", "format=wav; codec=pcm; sample_rate=16000; bit_rate=16; channel=1; language=" + _language);
            http.setTimeout(30000);
            
            int httpCode = http.POST(wavBuffer, wavSize);
            
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
                _sttProvider = providers[i];
                log_i("HAAssist: Found working STT provider: %s", providers[i]);
                
                String response = http.getString();
                http.end();
                free(wavBuffer);
                return parseSTTResponse(response);
            }
            
            log_w("HAAssist: Provider %s returned HTTP %d", providers[i], httpCode);
            http.end();
        }
        
        // No provider worked - maybe STT isn't set up
        _lastError = "No STT provider found. Check HA Assist configuration.";
        log_e("HAAssist: %s", _lastError.c_str());
        free(wavBuffer);
        return false;
    }
    
    // Use configured provider - _sttProvider already has just the name part
    // The API endpoint is /api/stt/stt.{provider}
    String url = _baseUrl + "/api/stt/stt." + _sttProvider;
    log_i("HAAssist: POST to %s", url.c_str());
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + _token);
    http.addHeader("Content-Type", "audio/wav");
    http.addHeader("X-Speech-Content", "format=wav; codec=pcm; sample_rate=16000; bit_rate=16; channel=1; language=" + _language);
    http.setTimeout(30000);
    
    int httpCode = http.POST(wavBuffer, wavSize);
    free(wavBuffer);
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String response = http.getString();
        http.end();
        return parseSTTResponse(response);
    } else {
        String errorBody = http.getString();
        _lastError = "STT failed: HTTP " + String(httpCode) + " - " + errorBody;
        log_e("HAAssist: %s", _lastError.c_str());
        http.end();
        return false;
    }
}

bool HAAssistClient::parseSTTResponse(const String& response) {
    log_i("HAAssist: STT response: %s", response.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        // Try plain text response (some STT returns just text)
        _lastTranscription = response;
        _lastTranscription.trim();
    } else {
        // JSON response - try different formats
        if (doc["text"].is<const char*>()) {
            _lastTranscription = doc["text"].as<String>();
        } else if (doc["result"].is<const char*>()) {
            _lastTranscription = doc["result"].as<String>();
        } else if (doc["speech"].is<const char*>()) {
            _lastTranscription = doc["speech"].as<String>();
        } else {
            _lastTranscription = response;
        }
    }
    
    log_i("HAAssist: Transcription: '%s'", _lastTranscription.c_str());
    return _lastTranscription.length() > 0;
}

bool HAAssistClient::sendToConversation(const char* text) {
    log_i("HAAssist: Sending to conversation: '%s'", text);
    
    JsonDocument doc;
    doc["text"] = text;
    doc["language"] = _language;
    
    if (_pipelineId.length() > 0) {
        doc["pipeline"] = _pipelineId;
    }
    
    String response = makeJsonRequest("/api/conversation/process", doc);
    
    if (response.length() == 0) {
        return false;
    }
    
    // Parse response
    JsonDocument respDoc;
    DeserializationError error = deserializeJson(respDoc, response);
    
    if (error) {
        _lastError = "Failed to parse conversation response";
        log_e("HAAssist: %s: %s", _lastError.c_str(), error.c_str());
        return false;
    }
    
    // Extract response text
    // Response format: {"response": {"speech": {"plain": {"speech": "..."}}}, "conversation_id": "..."}
    if (respDoc.containsKey("response")) {
        JsonObject resp = respDoc["response"];
        if (resp.containsKey("speech")) {
            JsonObject speech = resp["speech"];
            if (speech.containsKey("plain")) {
                _lastResponse = speech["plain"]["speech"].as<String>();
            }
        }
    }
    
    if (_lastResponse.length() == 0) {
        // Try alternate response format
        if (respDoc.containsKey("speech")) {
            _lastResponse = respDoc["speech"]["plain"]["speech"].as<String>();
        } else {
            _lastResponse = "(No response)";
        }
    }
    
    log_i("HAAssist: Response: '%s'", _lastResponse.c_str());
    return true;
}

String HAAssistClient::makeJsonRequest(const char* endpoint, JsonDocument& doc) {
    HTTPClient http;
    String url = _baseUrl + endpoint;
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + _token);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);
    
    String body;
    serializeJson(doc, body);
    
    log_d("HAAssist: POST %s: %s", url.c_str(), body.c_str());
    
    int httpCode = http.POST(body);
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String response = http.getString();
        http.end();
        return response;
    } else {
        _lastError = "HTTP " + String(httpCode) + ": " + http.getString();
        log_e("HAAssist: Request failed: %s", _lastError.c_str());
        http.end();
        return "";
    }
}

String HAAssistClient::makeRequest(const char* endpoint, const char* method, 
                                    const char* contentType, const uint8_t* body, size_t bodyLen) {
    HTTPClient http;
    String url = _baseUrl + endpoint;
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + _token);
    if (contentType) {
        http.addHeader("Content-Type", contentType);
    }
    http.setTimeout(15000);
    
    int httpCode;
    if (strcmp(method, "POST") == 0) {
        httpCode = http.POST((uint8_t*)body, bodyLen);
    } else if (strcmp(method, "GET") == 0) {
        httpCode = http.GET();
    } else {
        httpCode = http.sendRequest(method, (uint8_t*)body, bodyLen);
    }
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String response = http.getString();
        http.end();
        return response;
    } else {
        _lastError = "HTTP " + String(httpCode);
        http.end();
        return "";
    }
}

void HAAssistClient::loop() {
    // Currently synchronous, but this can be used for async operations in the future
}
