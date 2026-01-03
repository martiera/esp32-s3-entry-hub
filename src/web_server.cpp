#include "web_server.h"
#include "config.h"
#include "storage_manager.h"
#include "mqtt_client.h"
#include "audio_handler.h"
#include "voice_activity_handler.h"
#include "notification_manager.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>

WebServerManager webServer;

// Buffer for accumulating body data
static String bodyBuffer;

WebServerManager::WebServerManager() : server(WEB_SERVER_PORT), ws("/ws") {
}

void WebServerManager::begin() {
    Serial.println("Starting web server...");
    
    setupWebSocket();
    setupAPIEndpoints();  // Register API handlers BEFORE static files
    setupRoutes();
    
    server.begin();
    Serial.printf("Web server started on port %d\n", WEB_SERVER_PORT);
}

void WebServerManager::loop() {
    ws.cleanupClients();
}

void WebServerManager::setupWebSocket() {
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
}

void WebServerManager::setupRoutes() {
    // Serve static files from LittleFS, but only for non-API routes
    server.onNotFound([this](AsyncWebServerRequest *request) {
        // Don't try to serve static files for API routes
        if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"API endpoint not found\"}");
            return;
        }
        
        // Try to serve static file
        String path = request->url();
        if (path.endsWith("/")) path += "index.html";
        
        // Serve from /www/ directory in LittleFS
        String filePath = "/www" + path;
        if (LittleFS.exists(filePath)) {
            request->send(LittleFS, filePath);
        } else if (LittleFS.exists(filePath + ".gz")) {
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, (filePath + ".gz").c_str(), String(), false);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        } else {
            // Fallback to index.html for SPA routing
            request->send(LittleFS, "/www/index.html");
        }
    });
}

void WebServerManager::setupAPIEndpoints() {
            // MQTT validation endpoint
    server.on("/api/mqtt/validate", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleValidateMqtt(request);
    });
    
    // MQTT test connection endpoint
    server.on("/api/mqtt/test", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleTestMqtt(request);
    });
    // System status
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetStatus(request);
    });
    
    // Configuration
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetConfig(request);
    });
    
    // Specific config routes MUST come before general /api/config POST
    server.on("/api/config/weather", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSaveWeatherConfig(request, data, len);
        });
    
    server.on("/api/config/homeassistant", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleSaveHomeAssistantConfig(request, data, len);
        });
    
    server.on("/api/homeassistant/test", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleCheckHomeAssistantConnection(request);
    });
    
    server.on("/api/homeassistant/persons", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetHomeAssistantPersons(request);
    });
    
    server.on("/api/homeassistant/weather", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetHomeAssistantWeatherEntities(request);
    });
    
    server.on("/api/homeassistant/calendars", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetHomeAssistantCalendarEntities(request);
    });
    
    server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handlePostConfig(request, data, len, index, total);
        });
    
    // Voice commands
    server.on("/api/commands", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetCommands(request);
    });
    
    server.on("/api/commands", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostCommand(request);
    });
    
    server.on("/api/commands/delete", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleDeleteCommand(request);
    });
    
    // Presence
    server.on("/api/presence", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetPresence(request);
    });
    
    // Scenes
    server.on("/api/scene", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostScene(request);
    });
    
    // Weather
    server.on("/api/weather", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetWeather(request);
    });
    
    // Calendar
    server.on("/api/calendar", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetCalendar(request);
    });
    
    // Notifications
    server.on("/api/notifications/acknowledge", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleAcknowledgeNotification(request);
    });
    
    server.on("/api/notifications/test", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleTestNotification(request);
    });
    
    server.on("/api/notifications/active", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetActiveNotification(request);
    });
}

void WebServerManager::handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/www/index.html", "text/html");
}

void WebServerManager::handleGetStatus(AsyncWebServerRequest *request) {
    JsonDocument doc;
    
    doc["device"]["name"] = DEVICE_NAME;
    doc["device"]["version"] = DEVICE_VERSION;
    doc["device"]["uptime"] = millis() / 1000;
    doc["device"]["free_heap"] = ESP.getFreeHeap();
    
    doc["wifi"]["connected"] = WiFi.isConnected();
    doc["wifi"]["ssid"] = WiFi.SSID();
    doc["wifi"]["ip"] = WiFi.localIP().toString();
    doc["wifi"]["rssi"] = WiFi.RSSI();
    
    doc["mqtt"]["connected"] = mqttClient.isConnected();
    
    doc["audio"]["recording"] = audioHandler.isRecording();
    doc["audio"]["buffer_size"] = audioHandler.getBufferSize();
    
    doc["voice"]["mode"] = voiceActivity.getWakeMode() == WAKE_MODE_THRESHOLD ? "threshold" : "manual";
    doc["voice"]["active"] = voiceActivity.isVoiceDetected();
    doc["voice"]["audio_level"] = voiceActivity.getLastAudioLevel();
    doc["voice"]["threshold"] = voiceActivity.getThreshold();
    
    doc["storage"]["total"] = storage.getTotalSpace();
    doc["storage"]["used"] = storage.getUsedSpace();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetConfig(AsyncWebServerRequest *request) {
    JsonDocument doc;
    
    if (storage.loadConfig(doc)) {
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
    }
}

void WebServerManager::handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Accumulate body data
    if (index == 0) {
        bodyBuffer = "";
        bodyBuffer.reserve(total);
    }
    
    for (size_t i = 0; i < len; i++) {
        bodyBuffer += (char)data[i];
    }
    
    // Only process when we have all the data
    if (index + len != total) {
        return;
    }
    
    Serial.printf("Received complete config body (%d bytes)\n", bodyBuffer.length());
    
    JsonDocument newConfig;
    DeserializationError error = deserializeJson(newConfig, bodyBuffer);
    
    if (error) {
        Serial.printf("Config parse error: %s\n", error.c_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        bodyBuffer = "";
        return;
    }
    
    // Load existing config to preserve fields not in request if needed
    // But here we assume the client sends the full config or we merge carefully
    // For simplicity, let's load existing and merge top-level keys
    
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        Serial.println("Warning: Could not load existing config, using defaults");
        config["device"]["name"] = DEVICE_NAME;
    }
    
    // Merge known sections (using modern ArduinoJson syntax)
    if (!newConfig["device"].isNull()) config["device"] = newConfig["device"];
    if (!newConfig["network"].isNull()) config["network"] = newConfig["network"];
    if (!newConfig["mqtt"].isNull()) {
        config["mqtt"] = newConfig["mqtt"];
        // Reset validated flag on config change
        config["mqtt"]["validated"] = false;
        Serial.println("MQTT config updated, validated flag reset");
    }
    if (!newConfig["voice"].isNull()) config["voice"] = newConfig["voice"];
    if (!newConfig["display"].isNull()) config["display"] = newConfig["display"];
    if (!newConfig["weather"].isNull()) config["weather"] = newConfig["weather"];
    if (!newConfig["integrations"].isNull()) config["integrations"] = newConfig["integrations"];
    if (!newConfig["presence"].isNull()) config["presence"] = newConfig["presence"];
    
    Serial.println("Attempting to save config...");
    if (storage.saveConfig(config)) {
        Serial.println("Config saved successfully");
        request->send(200, "application/json", "{\"success\":true}");
        // Notify clients of config change
        broadcastMessage("config_updated", "Configuration updated");
    } else {
        Serial.println("Failed to save config to storage");
        request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
    }
    bodyBuffer = ""; // Clear buffer
}

void WebServerManager::handleGetCommands(AsyncWebServerRequest *request) {
    JsonDocument doc;
    
    if (storage.loadCommands(doc)) {
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        // Return empty commands array
        request->send(200, "application/json", "{\"commands\":[]}");
    }
}

void WebServerManager::handlePostCommand(AsyncWebServerRequest *request) {
    // TODO: Implement command addition/update
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleDeleteCommand(AsyncWebServerRequest *request) {
    // TODO: Implement command deletion
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleGetPresence(AsyncWebServerRequest *request) {
    JsonDocument config;
    
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    
    // Check if person tracking is configured
    if (!config["presence"]["home_assistant"]["entity_ids"].is<JsonArray>() || 
        config["presence"]["home_assistant"]["entity_ids"].as<JsonArray>().size() == 0) {
        request->send(200, "application/json", "{\"people\":[]}");
        return;
    }
    
    // Fetch from Home Assistant
    handleHomeAssistantPersons(request, config);
}

void WebServerManager::handleHomeAssistantPersons(AsyncWebServerRequest *request, JsonDocument &config) {
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    JsonArray entityIds = config["presence"]["home_assistant"]["entity_ids"].as<JsonArray>();
    
    if (!haUrl || !haToken || entityIds.size() == 0) {
        request->send(400, "application/json", "{\"error\":\"Person tracking not configured\"}");
        return;
    }
    
    JsonDocument response;
    JsonArray people = response["people"].to<JsonArray>();
    
    // Fetch each person entity from Home Assistant
    for (JsonVariant entityId : entityIds) {
        String url = String(haUrl);
        if (!url.endsWith("/")) url += "/";
        url += "api/states/";
        url += entityId.as<String>();
        
        HTTPClient http;
        http.begin(url);
        http.addHeader("Authorization", String("Bearer ") + haToken);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(3000);
        
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            String payload = http.getString();
            JsonDocument personDoc;
            deserializeJson(personDoc, payload);
            
            // Extract person data
            JsonDocument person;
            String entityIdStr = entityId.as<String>();
            String name;
            
            if (personDoc["attributes"]["friendly_name"]) {
                name = personDoc["attributes"]["friendly_name"].as<String>();
            } else {
                name = entityIdStr.substring(entityIdStr.indexOf('.') + 1);
                if (name.length() > 0) name[0] = toupper(name[0]); // Capitalize first letter
            }
            
            person["entity_id"] = entityIdStr;
            person["name"] = name;
            person["present"] = (personDoc["state"].as<String>() == "home");
            person["location"] = personDoc["state"].as<String>();
            
            // Get avatar from attributes if available
            if (personDoc["attributes"]["icon"]) {
                person["avatar"] = personDoc["attributes"]["icon"].as<String>();
            } else {
                // Default avatars based on entity_id
                if (entityIdStr.indexOf("john") >= 0 || entityIdStr.indexOf("dad") >= 0) {
                    person["avatar"] = "👨";
                } else if (entityIdStr.indexOf("jane") >= 0 || entityIdStr.indexOf("mom") >= 0) {
                    person["avatar"] = "👩";
                } else if (entityIdStr.indexOf("kid") >= 0 || entityIdStr.indexOf("child") >= 0) {
                    person["avatar"] = "👶";
                } else {
                    person["avatar"] = "👤";
                }
            }
            
            // Get additional attributes
            if (personDoc["attributes"]["latitude"]) {
                person["latitude"] = personDoc["attributes"]["latitude"].as<float>();
                person["longitude"] = personDoc["attributes"]["longitude"].as<float>();
            }
            if (personDoc["attributes"]["source"]) {
                person["source"] = personDoc["attributes"]["source"].as<String>();
            }
            
            people.add(person);
        }
        
        http.end();
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    request->send(200, "application/json", responseStr);
}

void WebServerManager::handlePostScene(AsyncWebServerRequest *request) {
    // TODO: Implement scene triggering
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleSaveWeatherConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    JsonDocument newWeatherConfig;
    DeserializationError error = deserializeJson(newWeatherConfig, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Load existing config
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        config["device"]["name"] = DEVICE_NAME;
    }
    
    // Update weather section - preserve existing structure
    if (!newWeatherConfig["provider"].isNull()) {
        config["weather"]["provider"] = newWeatherConfig["provider"].as<String>();
    } else {
        config["weather"]["provider"] = "none";
    }
    
    if (newWeatherConfig["openweathermap"].is<JsonObject>()) {
        config["weather"]["openweathermap"] = newWeatherConfig["openweathermap"];
    }
    if (newWeatherConfig["home_assistant"].is<JsonObject>()) {
        config["weather"]["home_assistant"] = newWeatherConfig["home_assistant"];
    }
    
    // Save config
    if (storage.saveConfig(config)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
    }
}

void WebServerManager::handleSaveHomeAssistantConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    JsonDocument newHAConfig;
    DeserializationError error = deserializeJson(newHAConfig, data, len);
    
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Load existing config
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        config["device"]["name"] = DEVICE_NAME;
    }
    
    // Update Home Assistant integration section
    config["integrations"]["home_assistant"] = newHAConfig;
    
    // Save config
    if (storage.saveConfig(config)) {
        request->send(200, "application/json", "{\"success\":true}");
        Serial.println("Home Assistant configuration updated");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
    }
}

void WebServerManager::handleCheckHomeAssistantConnection(AsyncWebServerRequest *request) {
    JsonDocument config;
    
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    
    if (!haUrl || strlen(haUrl) == 0) {
        request->send(400, "application/json", "{\"connected\":false,\"error\":\"URL not configured\"}");
        return;
    }
    
    if (!haToken || strlen(haToken) == 0) {
        request->send(400, "application/json", "{\"connected\":false,\"error\":\"Token not configured\"}");
        return;
    }
    
    // Test connection by getting HA config
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/config";
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + haToken);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        // Parse HA config response
        JsonDocument haConfig;
        deserializeJson(haConfig, payload);
        
        JsonDocument response;
        response["connected"] = true;
        response["version"] = haConfig["version"].as<String>();
        response["location_name"] = haConfig["location_name"].as<String>();
        response["url"] = haUrl;
        
        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
    } else {
        http.end();
        String errorResponse = "{\"connected\":false,\"error\":\"Authentication failed\",\"code\":";
        errorResponse += httpCode;
        errorResponse += "}";
        request->send(httpCode == 401 || httpCode == 403 ? 401 : 500, "application/json", errorResponse);
    }
}

void WebServerManager::handleGetHomeAssistantPersons(AsyncWebServerRequest *request) {
    JsonDocument config;
    
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    
    if (!haUrl || strlen(haUrl) == 0 || !haToken || strlen(haToken) == 0) {
        request->send(400, "application/json", "{\"error\":\"Home Assistant not configured. Please configure HA integration first.\"}");
        return;
    }
    
    // Use HA Template API to get only person entities - much smaller response!
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/template";
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + haToken);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    
    // Template to get person entities with their state and friendly name
    String templatePayload = "{\"template\":\"[{% for person in states.person %}{\\\"entity_id\\\":\\\"{{ person.entity_id }}\\\",\\\"state\\\":\\\"{{ person.state }}\\\",\\\"name\\\":\\\"{{ person.attributes.friendly_name | default(person.name) }}\\\"}{% if not loop.last %},{% endif %}{% endfor %}]\"}";
    
    int httpCode = http.POST(templatePayload);
    
    if (httpCode != 200) {
        http.end();
        String errorResponse = "{\"error\":\"Failed to connect to Home Assistant\",\"code\":";
        errorResponse += httpCode;
        errorResponse += "}";
        request->send(httpCode, "application/json", errorResponse);
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    // Parse the template response (it's a JSON array of person entities)
    JsonDocument persons;
    DeserializationError error = deserializeJson(persons, payload);
    
    if (error) {
        String errorMsg = "{\"error\":\"Failed to parse Home Assistant response: ";
        errorMsg += error.c_str();
        errorMsg += "\"}";
        request->send(500, "application/json", errorMsg);
        return;
    }
    
    // Wrap in response object
    JsonDocument response;
    response["persons"] = persons;
    
    String responseStr;
    serializeJson(response, responseStr);
    request->send(200, "application/json", responseStr);
}

void WebServerManager::handleGetHomeAssistantWeatherEntities(AsyncWebServerRequest *request) {
    JsonDocument config;
    
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    
    if (!haUrl || strlen(haUrl) == 0 || !haToken || strlen(haToken) == 0) {
        request->send(400, "application/json", "{\"error\":\"Home Assistant not configured\"}");
        return;
    }
    
    // Use HA Template API to get weather entities
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/template";
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + haToken);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    
    // Template to get weather entities with their friendly name
    String templatePayload = "{\"template\":\"[{% for weather in states.weather %}{\\\"entity_id\\\":\\\"{{ weather.entity_id }}\\\",\\\"state\\\":\\\"{{ weather.state }}\\\",\\\"name\\\":\\\"{{ weather.attributes.friendly_name | default(weather.name) }}\\\"}{% if not loop.last %},{% endif %}{% endfor %}]\"}";
    
    int httpCode = http.POST(templatePayload);
    
    if (httpCode != 200) {
        http.end();
        request->send(httpCode, "application/json", "{\"error\":\"Failed to connect to Home Assistant\"}");
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    JsonDocument entities;
    DeserializationError error = deserializeJson(entities, payload);
    
    if (error) {
        request->send(500, "application/json", "{\"error\":\"Failed to parse response\"}");
        return;
    }
    
    JsonDocument response;
    response["entities"] = entities;
    
    String responseStr;
    serializeJson(response, responseStr);
    request->send(200, "application/json", responseStr);
}

void WebServerManager::handleGetHomeAssistantCalendarEntities(AsyncWebServerRequest *request) {
    JsonDocument config;
    
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    
    if (!haUrl || strlen(haUrl) == 0 || !haToken || strlen(haToken) == 0) {
        request->send(400, "application/json", "{\"error\":\"Home Assistant not configured\"}");
        return;
    }
    
    // Use HA Template API to get calendar entities
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/template";
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + haToken);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);
    
    // Template to get calendar entities with their friendly name
    String templatePayload = "{\"template\":\"[{% for cal in states.calendar %}{\\\"entity_id\\\":\\\"{{ cal.entity_id }}\\\",\\\"state\\\":\\\"{{ cal.state }}\\\",\\\"name\\\":\\\"{{ cal.attributes.friendly_name | default(cal.name) }}\\\"}{% if not loop.last %},{% endif %}{% endfor %}]\"}";
    
    int httpCode = http.POST(templatePayload);
    
    if (httpCode != 200) {
        http.end();
        request->send(httpCode, "application/json", "{\"error\":\"Failed to connect to Home Assistant\"}");
        return;
    }
    
    String payload = http.getString();
    http.end();
    
    JsonDocument entities;
    DeserializationError error = deserializeJson(entities, payload);
    
    if (error) {
        request->send(500, "application/json", "{\"error\":\"Failed to parse response\"}");
        return;
    }
    
    JsonDocument response;
    response["entities"] = entities;
    
    String responseStr;
    serializeJson(response, responseStr);
    request->send(200, "application/json", responseStr);
}

void WebServerManager::handleGetWeather(AsyncWebServerRequest *request) {
    JsonDocument config;
    
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    
    const char* provider = config["weather"]["provider"] | "none";
    
    if (strcmp(provider, "none") == 0) {
        request->send(200, "application/json", "{\"configured\":false,\"message\":\"Weather provider not configured\"}");
        return;
    }
    
    if (strcmp(provider, "openweathermap") == 0) {
        handleOpenWeatherMap(request, config);
    } else if (strcmp(provider, "homeassistant") == 0) {
        handleHomeAssistantWeather(request, config);
    } else {
        request->send(400, "application/json", "{\"error\":\"Unknown weather provider\"}");
    }
}

void WebServerManager::handleOpenWeatherMap(AsyncWebServerRequest *request, JsonDocument& config) {
    const char* apiKey = config["weather"]["openweathermap"]["api_key"];
    const char* city = config["weather"]["openweathermap"]["city"];
    const char* units = config["weather"]["openweathermap"]["units"] | "metric";
    
    if (!apiKey || strlen(apiKey) == 0 || !city || strlen(city) == 0) {
        request->send(400, "application/json", "{\"error\":\"OpenWeatherMap not configured\"}");
        return;
    }
    
    // Build URL
    String url = "http://api.openweathermap.org/data/2.5/weather?q=";
    url += city;
    url += "&appid=";
    url += apiKey;
    url += "&units=";
    url += units;
    
    // Make HTTP request
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        request->send(200, "application/json", payload);
    } else {
        http.end();
        String error = "{\"error\":\"Failed to fetch weather\",\"code\":";
        error += httpCode;
        error += "}";
        request->send(500, "application/json", error);
    }
}

void WebServerManager::handleHomeAssistantWeather(AsyncWebServerRequest *request, JsonDocument& config) {
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    const char* entityId = config["weather"]["home_assistant"]["entity_id"] | "weather.forecast_home";
    
    if (!haUrl || strlen(haUrl) == 0) {
        request->send(400, "application/json", "{\"error\":\"Home Assistant URL not configured\"}");
        return;
    }
    
    // Build URL
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/states/";
    url += entityId;
    
    // Make HTTP request
    HTTPClient http;
    http.begin(url);
    
    // Add authorization header if token is provided
    if (haToken && strlen(haToken) > 0) {
        String auth = "Bearer ";
        auth += haToken;
        http.addHeader("Authorization", auth);
    }
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        // Parse and transform HA response to standard format
        JsonDocument haDoc;
        deserializeJson(haDoc, payload);
        
        JsonDocument weather;
        weather["state"] = haDoc["state"].as<String>();
        weather["temperature"] = haDoc["attributes"]["temperature"];
        weather["humidity"] = haDoc["attributes"]["humidity"];
        weather["pressure"] = haDoc["attributes"]["pressure"];
        weather["wind_speed"] = haDoc["attributes"]["wind_speed"];
        weather["description"] = haDoc["state"].as<String>();
        weather["provider"] = "homeassistant";
        
        String response;
        serializeJson(weather, response);
        request->send(200, "application/json", response);
    } else {
        http.end();
        String error = "{\"error\":\"Failed to fetch weather from Home Assistant\",\"code\":";
        error += httpCode;
        error += "}";
        request->send(500, "application/json", error);
    }
}

void WebServerManager::handleGetCalendar(AsyncWebServerRequest *request) {
    JsonDocument config;
    
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    
    // Check if calendar is enabled
    bool enabled = config["integrations"]["calendar"]["enabled"] | false;
    const char* provider = config["integrations"]["calendar"]["provider"] | "none";
    
    if (!enabled || strcmp(provider, "none") == 0) {
        request->send(200, "application/json", "{\"configured\":false,\"error\":\"Calendar not configured\"}");
        return;
    }
    
    if (strcmp(provider, "homeassistant") == 0) {
        handleHomeAssistantCalendar(request, config);
    } else {
        request->send(400, "application/json", "{\"error\":\"Unknown calendar provider\"}");
    }
}

void WebServerManager::handleHomeAssistantCalendar(AsyncWebServerRequest *request, JsonDocument& config) {
    const char* haUrl = config["integrations"]["home_assistant"]["url"];
    const char* haToken = config["integrations"]["home_assistant"]["token"];
    const char* entityId = config["integrations"]["calendar"]["home_assistant"]["entity_id"] | "calendar.family";
    int maxEvents = config["integrations"]["calendar"]["home_assistant"]["max_events"] | 5;
    
    if (!haUrl || strlen(haUrl) == 0) {
        request->send(400, "application/json", "{\"error\":\"Home Assistant URL not configured\"}");
        return;
    }
    
    if (!haToken || strlen(haToken) == 0) {
        request->send(400, "application/json", "{\"error\":\"Home Assistant token not configured\"}");
        return;
    }
    
    // Build URL for calendar events
    // HA calendar API: /api/calendars/<entity_id>?start=<datetime>&end=<datetime>
    String url = String(haUrl);
    if (!url.endsWith("/")) url += "/";
    url += "api/calendars/";
    url += entityId;
    
    // Get events for today and next 7 days
    // Format: YYYY-MM-DD (simpler format that HA accepts)
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    char startDate[12];
    char endDate[12];
    
    // Start: today
    snprintf(startDate, sizeof(startDate), "%04d-%02d-%02d",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
    
    // End: 7 days from now
    time_t endTime = now + (7 * 24 * 60 * 60);
    struct tm* endTimeinfo = localtime(&endTime);
    snprintf(endDate, sizeof(endDate), "%04d-%02d-%02d",
             endTimeinfo->tm_year + 1900, endTimeinfo->tm_mon + 1, endTimeinfo->tm_mday);
    
    url += "?start=";
    url += startDate;
    url += "&end=";
    url += endDate;
    
    Serial.printf("Calendar API URL: %s\n", url.c_str());
    
    // Make HTTP request
    HTTPClient http;
    http.begin(url);
    
    String auth = "Bearer ";
    auth += haToken;
    http.addHeader("Authorization", auth);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.GET();
    Serial.printf("Calendar API response code: %d\n", httpCode);
    
    if (httpCode == 200) {
        String payload = http.getString();
        http.end();
        
        Serial.printf("Calendar API response: %s\n", payload.substring(0, 500).c_str());
        
        // Parse HA calendar response
        JsonDocument haDoc;
        DeserializationError error = deserializeJson(haDoc, payload);
        
        if (error) {
            Serial.printf("Calendar JSON parse error: %s\n", error.c_str());
            request->send(500, "application/json", "{\"error\":\"Failed to parse calendar response\"}");
            return;
        }
        
        // Transform to our format and limit events
        JsonDocument response;
        response["provider"] = "homeassistant";
        response["entity_id"] = entityId;
        
        JsonArray events = response["events"].to<JsonArray>();
        int count = 0;
        
        // HA returns array of events directly
        JsonArray haEvents = haDoc.as<JsonArray>();
        Serial.printf("Calendar events count from HA: %d\n", haEvents.size());
        
        for (JsonVariant event : haEvents) {
            if (count >= maxEvents) break;
            
            JsonObject evt = events.add<JsonObject>();
            evt["summary"] = event["summary"];
            
            // Handle both dateTime and date formats
            if (event["start"]["dateTime"]) {
                evt["start"] = event["start"]["dateTime"];
                evt["all_day"] = false;
            } else if (event["start"]["date"]) {
                evt["start"] = event["start"]["date"];
                evt["all_day"] = true;
            } else {
                // Fallback - try direct start value
                evt["start"] = event["start"];
                evt["all_day"] = false;
            }
            
            if (event["end"]["dateTime"]) {
                evt["end"] = event["end"]["dateTime"];
            } else if (event["end"]["date"]) {
                evt["end"] = event["end"]["date"];
            } else {
                evt["end"] = event["end"];
            }
            
            if (event["description"]) {
                evt["description"] = event["description"];
            }
            if (event["location"]) {
                evt["location"] = event["location"];
            }
            
            count++;
        }
        
        String output;
        serializeJson(response, output);
        request->send(200, "application/json", output);
    } else {
        String errorBody = http.getString();
        http.end();
        Serial.printf("Calendar API error: %s\n", errorBody.c_str());
        
        String errorMsg;
        if (httpCode == 404) {
            errorMsg = "{\"error\":\"Calendar entity not found. Check entity ID.\"}";
        } else if (httpCode == 401) {
            errorMsg = "{\"error\":\"Unauthorized. Check HA token.\"}";
        } else {
            errorMsg = "{\"error\":\"Failed to fetch calendar\",\"code\":";
            errorMsg += httpCode;
            errorMsg += "}";
        }
        request->send(httpCode >= 400 && httpCode < 500 ? httpCode : 500, "application/json", errorMsg);
    }
}

void WebServerManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // Handle incoming WebSocket messages
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebServerManager::broadcastStatus(const JsonDocument& doc) {
    String message;
    serializeJson(doc, message);
    ws.textAll(message);
}

void WebServerManager::broadcastMessage(const char* type, const char* message) {
    JsonDocument doc;
    doc["type"] = type;
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

void WebServerManager::handleValidateMqtt(AsyncWebServerRequest *request) {
    JsonDocument config;
    if (!storage.loadConfig(config)) {
        request->send(500, "application/json", "{\"error\":\"Failed to load config\"}");
        return;
    }
    config["mqtt"]["validated"] = true;
    if (storage.saveConfig(config)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Failed to save config\"}");
    }
}

void WebServerManager::handleTestMqtt(AsyncWebServerRequest *request) {
    // Reload config and trigger reconnection
    mqttClient.forceReconnect();
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleAcknowledgeNotification(AsyncWebServerRequest *request) {
    notificationManager.acknowledge();
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleTestNotification(AsyncWebServerRequest *request) {
    notificationManager.notifyCustom("Test notification from web UI", LedColor::Purple(), LedPattern::PULSE);
    request->send(200, "application/json", "{\"success\":true}");
}

void WebServerManager::handleGetActiveNotification(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["active"] = notificationManager.hasActiveNotification();
    doc["message"] = notificationManager.getCurrentNotification();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
