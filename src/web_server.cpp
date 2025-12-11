#include "web_server.h"
#include "config.h"
#include "storage_manager.h"
#include "mqtt_client.h"
#include "audio_handler.h"
#include "porcupine_handler.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>

WebServerManager webServer;

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
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, filePath + ".gz");
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        } else {
            // Fallback to index.html for SPA routing
            request->send(LittleFS, "/www/index.html");
        }
    });
}

void WebServerManager::setupAPIEndpoints() {
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
    
    server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePostConfig(request);
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
    
    doc["voice"]["wake_word"] = porcupineHandler.getDetectedKeyword();
    doc["voice"]["active"] = porcupineHandler.isWakeWordDetected();
    
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

void WebServerManager::handlePostConfig(AsyncWebServerRequest *request) {
    // TODO: Implement configuration update
    request->send(200, "application/json", "{\"success\":true}");
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
    if (!config["presence"]["enabled"].as<bool>() || 
        !config["presence"]["home_assistant"]["entity_ids"].is<JsonArray>()) {
        // Return static data from presence.json
        JsonDocument doc;
        if (storage.loadPresence(doc)) {
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        } else {
            request->send(200, "application/json", "{\"people\":[]}");
        }
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
            String name = entityIdStr.substring(entityIdStr.indexOf('.') + 1);
            name[0] = toupper(name[0]); // Capitalize first letter
            
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
