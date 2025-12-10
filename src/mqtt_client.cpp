#include "mqtt_client.h"
#include "config.h"
#include "secrets.h"

MQTTClientManager mqttClient;
static MqttMessageCallback globalCallback = nullptr;

MQTTClientManager::MQTTClientManager() 
    : client(espClient), messageCallback(nullptr), lastReconnectAttempt(0) {
}

void MQTTClientManager::begin() {
    client.setServer(MQTT_BROKER, MQTT_PORT);
    client.setCallback(mqttCallback);
    client.setBufferSize(MQTT_BUFFER_SIZE);
    client.setKeepAlive(MQTT_KEEPALIVE);
    
    Serial.println("MQTT Client initialized");
}

void MQTTClientManager::loop() {
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            reconnect();
        }
    } else {
        client.loop();
    }
}

bool MQTTClientManager::isConnected() {
    return client.connected();
}

void MQTTClientManager::reconnect() {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = String(DEVICE_NAME) + "-" + String(ESP.getEfuseMac(), HEX);
    
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("connected");
        
        // Subscribe to command topics
        subscribe("entryhub/command");
        subscribe("entryhub/config");
        subscribe("homeassistant/+/+/state");
        
        // Publish online status
        publishStatus("online");
        
    } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" retrying later");
    }
}

bool MQTTClientManager::publish(const char* topic, const char* payload, bool retained) {
    if (!client.connected()) {
        return false;
    }
    return client.publish(topic, payload, retained);
}

bool MQTTClientManager::publishJson(const char* topic, JsonDocument& doc, bool retained) {
    String output;
    serializeJson(doc, output);
    return publish(topic, output.c_str(), retained);
}

bool MQTTClientManager::subscribe(const char* topic) {
    if (!client.connected()) {
        return false;
    }
    Serial.print("Subscribing to: ");
    Serial.println(topic);
    return client.subscribe(topic);
}

void MQTTClientManager::setCallback(MqttMessageCallback callback) {
    messageCallback = callback;
    globalCallback = callback;
}

void MQTTClientManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Null-terminate payload
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.print("MQTT Message [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);
    
    if (globalCallback) {
        globalCallback(topic, message);
    }
}

String MQTTClientManager::getTopicPrefix() {
    return "entryhub";
}

void MQTTClientManager::publishStatus(const char* status) {
    JsonDocument doc;
    doc["status"] = status;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["version"] = DEVICE_VERSION;
    
    publishJson("entryhub/status", doc, true);
}

void MQTTClientManager::publishVoiceDetection(const char* wakeWord) {
    JsonDocument doc;
    doc["wake_word"] = wakeWord;
    doc["timestamp"] = millis();
    
    publishJson("entryhub/voice/detected", doc);
}

void MQTTClientManager::publishCommandExecuted(const char* command, const char* result) {
    JsonDocument doc;
    doc["command"] = command;
    doc["result"] = result;
    doc["timestamp"] = millis();
    
    publishJson("entryhub/command/executed", doc);
}

void MQTTClientManager::publishPresenceUpdate(const char* person, bool present) {
    JsonDocument doc;
    doc["person"] = person;
    doc["present"] = present;
    doc["timestamp"] = millis();
    
    publishJson("entryhub/presence/status", doc);
}
