#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>

class WebServerManager {
public:
    WebServerManager();
    void begin();
    void loop();
    
    void broadcastStatus(const JsonDocument& doc);
    void broadcastMessage(const char* type, const char* message);
    
    AsyncWebServer server; // Made public for OTA manager
    
private:
    AsyncWebSocket ws;
    
    void setupRoutes();
    void setupWebSocket();
    void setupAPIEndpoints();
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
    
    // API handlers
    void handleGetStatus(AsyncWebServerRequest *request);
    void handleGetConfig(AsyncWebServerRequest *request);
    void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleGetCommands(AsyncWebServerRequest *request);
    void handlePostCommand(AsyncWebServerRequest *request);
    void handleDeleteCommand(AsyncWebServerRequest *request);
    void handleGetPresence(AsyncWebServerRequest *request);
    void handleHomeAssistantPersons(AsyncWebServerRequest *request, JsonDocument &config);
    void handlePostScene(AsyncWebServerRequest *request);
    void handleGetWeather(AsyncWebServerRequest *request);
    void handleOpenWeatherMap(AsyncWebServerRequest *request, JsonDocument& config);
    void handleHomeAssistantWeather(AsyncWebServerRequest *request, JsonDocument& config);
    void handleGetCalendar(AsyncWebServerRequest *request);
    void handleHomeAssistantCalendar(AsyncWebServerRequest *request, JsonDocument& config);
    void handleSaveWeatherConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    void handleSaveHomeAssistantConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len);
    void handleCheckHomeAssistantConnection(AsyncWebServerRequest *request);
    void handleGetHomeAssistantPersons(AsyncWebServerRequest *request);
    void handleGetHomeAssistantWeatherEntities(AsyncWebServerRequest *request);
    void handleGetHomeAssistantCalendarEntities(AsyncWebServerRequest *request);
    void handleValidateMqtt(AsyncWebServerRequest *request);
    void handleTestMqtt(AsyncWebServerRequest *request);
    
    // WebSocket handlers
    static void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                                 AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
};

extern WebServerManager webServer;

#endif
