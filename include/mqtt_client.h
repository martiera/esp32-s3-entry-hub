#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

typedef void (*MqttMessageCallback)(const char* topic, const char* payload);

class MQTTClientManager {
public:
    MQTTClientManager();
    void begin();
    void loop();
    bool isConnected();
    bool isEnabled();
    void forceReconnect();
    
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool publishJson(const char* topic, JsonDocument& doc, bool retained = false);
    bool subscribe(const char* topic);
    
    void setCallback(MqttMessageCallback callback);
    
    // High-level publishing methods
    void publishStatus(const char* status);
    void publishVoiceDetection(const char* wakeWord);
    void publishCommandExecuted(const char* command, const char* result);
    void publishPresenceUpdate(const char* person, bool present);
    
private:
    WiFiClient espClient;
    PubSubClient client;
    MqttMessageCallback messageCallback;
    unsigned long lastReconnectAttempt;
    int reconnectFailures;
    // Config loaded from config.json
    bool mqttEnabled;
    char mqttBroker[64];
    uint16_t mqttPort;
    char mqttUsername[32];
    char mqttPassword[64];
    char mqttClientId[32];
    char mqttTopicPrefix[32];
    bool mqttValidated;
    void loadConfig();
    void reconnect();
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    String getTopicPrefix();
};

extern MQTTClientManager mqttClient;

#endif
