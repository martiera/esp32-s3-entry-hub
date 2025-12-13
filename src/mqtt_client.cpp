#include "mqtt_client.h"
#include "config.h"
#include "storage_manager.h"
#include "notification_manager.h"

MQTTClientManager mqttClient;
static MqttMessageCallback globalCallback = nullptr;

MQTTClientManager::MQTTClientManager() 
        : client(espClient), messageCallback(nullptr), lastReconnectAttempt(0), reconnectFailures(0),
            mqttEnabled(false), mqttPort(1883), mqttValidated(false), lastMqttState(false) {
        memset(mqttBroker, 0, sizeof(mqttBroker));
        memset(mqttUsername, 0, sizeof(mqttUsername));
        memset(mqttPassword, 0, sizeof(mqttPassword));
        strncpy(mqttClientId, "esp32-entry-hub", sizeof(mqttClientId) - 1);
        strncpy(mqttTopicPrefix, "entryhub", sizeof(mqttTopicPrefix) - 1);
}

void MQTTClientManager::loadConfig() {
    JsonDocument config;
    if (storage.loadConfig(config)) {
        mqttEnabled = config["mqtt"]["enabled"] | false;
        mqttValidated = config["mqtt"]["validated"] | false;
        const char* broker = config["mqtt"]["broker"];
        if (broker && strlen(broker) > 0) {
            strncpy(mqttBroker, broker, sizeof(mqttBroker) - 1);
        }
        mqttPort = config["mqtt"]["port"] | 1883;
        const char* username = config["mqtt"]["username"];
        if (username) {
            strncpy(mqttUsername, username, sizeof(mqttUsername) - 1);
        }
        const char* password = config["mqtt"]["password"];
        if (password) {
            strncpy(mqttPassword, password, sizeof(mqttPassword) - 1);
        }
        const char* clientId = config["mqtt"]["client_id"];
        if (clientId && strlen(clientId) > 0) {
            strncpy(mqttClientId, clientId, sizeof(mqttClientId) - 1);
        }
        const char* topicPrefix = config["mqtt"]["topic_prefix"];
        if (topicPrefix && strlen(topicPrefix) > 0) {
            strncpy(mqttTopicPrefix, topicPrefix, sizeof(mqttTopicPrefix) - 1);
        }
    }
    Serial.printf("MQTT Config: enabled=%d, broker=%s, port=%d, validated=%d\n", 
                  mqttEnabled, mqttBroker, mqttPort, mqttValidated);
}

void MQTTClientManager::begin() {
    loadConfig();
    
    if (!mqttEnabled || strlen(mqttBroker) == 0) {
        Serial.println("MQTT disabled or not configured");
        return;
    }
    
    client.setServer(mqttBroker, mqttPort);
    client.setCallback(mqttCallback);
    client.setBufferSize(MQTT_BUFFER_SIZE);
    client.setKeepAlive(MQTT_KEEPALIVE);
    
    Serial.printf("MQTT Client initialized - connecting to %s:%d\n", mqttBroker, mqttPort);
    
    // Attempt initial connection (regardless of validated flag)
    reconnect();
}

void MQTTClientManager::loop() {
    if (!mqttEnabled || strlen(mqttBroker) == 0) {
        return;
    }
    // Only auto-reconnect if validated
    if (!mqttValidated) {
        return;
    }
    bool currentState = client.connected();
    
    // Detect state change
    if (currentState != lastMqttState) {
        if (currentState) {
            Serial.println("MQTT reconnected!");
            notificationManager.notifyConnectionIssue("MQTT", true);
        } else {
            Serial.println("MQTT disconnected!");
            notificationManager.notifyConnectionIssue("MQTT", false);
        }
        lastMqttState = currentState;
    }
    
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            reconnect();
        }
    } else {
        client.loop();
        reconnectFailures = 0; // Reset on successful connection
    }
}

bool MQTTClientManager::isConnected() {
    return mqttEnabled && client.connected();
}

bool MQTTClientManager::isEnabled() {
    return mqttEnabled && strlen(mqttBroker) > 0;
}

void MQTTClientManager::reconnect() {
    if (!mqttEnabled || strlen(mqttBroker) == 0) {
        return;
    }
    // Limit reconnect attempts if not validated
    const int MAX_RECONNECT_FAILURES = 3;
    if (!mqttValidated && reconnectFailures >= MAX_RECONNECT_FAILURES) {
        Serial.println("MQTT not validated and max reconnect attempts reached. No further retries.");
        return;
    }
    Serial.print("Attempting MQTT connection to ");
    Serial.print(mqttBroker);
    Serial.print("...");
    // Use configured client ID or generate unique one
    String clientId = String(mqttClientId) + "-" + String(ESP.getEfuseMac() & 0xFFFF, HEX);
    bool connected = false;
    if (strlen(mqttUsername) > 0) {
        connected = client.connect(clientId.c_str(), mqttUsername, mqttPassword);
    } else {
        connected = client.connect(clientId.c_str());
    }
    if (connected) {
        Serial.println("connected");
        reconnectFailures = 0;
        // Subscribe to command topics using configured prefix
        String cmdTopic = String(mqttTopicPrefix) + "/command";
        String cfgTopic = String(mqttTopicPrefix) + "/config";
        subscribe(cmdTopic.c_str());
        subscribe(cfgTopic.c_str());
        subscribe("homeassistant/+/+/state");
        // Publish online status
        publishStatus("online");
    } else {
        reconnectFailures++;
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.print(" retrying later (failures: ");
        Serial.print(reconnectFailures);
        Serial.println(")");
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
    return String(mqttTopicPrefix);
}

void MQTTClientManager::publishStatus(const char* status) {
    JsonDocument doc;
    doc["status"] = status;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["version"] = DEVICE_VERSION;
    
    String topic = String(mqttTopicPrefix) + "/status";
    publishJson(topic.c_str(), doc, true);
}

void MQTTClientManager::publishVoiceDetection(const char* wakeWord) {
    JsonDocument doc;
    doc["wake_word"] = wakeWord;
    doc["timestamp"] = millis();
    
    String topic = String(mqttTopicPrefix) + "/voice/detected";
    publishJson(topic.c_str(), doc);
}

void MQTTClientManager::publishCommandExecuted(const char* command, const char* result) {
    JsonDocument doc;
    doc["command"] = command;
    doc["result"] = result;
    doc["timestamp"] = millis();
    
    String topic = String(mqttTopicPrefix) + "/command/executed";
    publishJson(topic.c_str(), doc);
}

void MQTTClientManager::publishPresenceUpdate(const char* person, bool present) {
    JsonDocument doc;
    doc["person"] = person;
    doc["present"] = present;
    doc["timestamp"] = millis();
    
    String topic = getTopicPrefix() + "/presence";
    publishJson(topic.c_str(), doc);
}

void MQTTClientManager::forceReconnect() {
    Serial.println("Force reconnecting MQTT...");
    reconnectFailures = 0; // Reset failure count for forced reconnect
    loadConfig(); // Reload config to get latest settings
    
    // Reconfigure client with new settings
    if (mqttEnabled && strlen(mqttBroker) > 0) {
        client.setServer(mqttBroker, mqttPort);
        reconnect();
    } else {
        Serial.println("MQTT not configured for connection");
    }
}
