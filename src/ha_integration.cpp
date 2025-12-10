#include "ha_integration.h"
#include "config.h"

HomeAssistantIntegration homeAssistant;

HomeAssistantIntegration::HomeAssistantIntegration() 
    : lastUpdate(0), discoveryPublished(false) {
}

void HomeAssistantIntegration::begin() {
    Serial.println("Initializing Home Assistant integration...");
    
    // Publish discovery messages after a delay
    delay(2000);
    publishDiscovery();
    
    Serial.println("Home Assistant integration ready");
}

void HomeAssistantIntegration::loop() {
    unsigned long now = millis();
    
    // Periodic state updates
    if (now - lastUpdate >= HA_UPDATE_INTERVAL) {
        lastUpdate = now;
        publishDeviceState();
    }
}

void HomeAssistantIntegration::publishDiscovery() {
    if (discoveryPublished) {
        return;
    }
    
    Serial.println("Publishing Home Assistant discovery...");
    
    // Publish sensor discoveries
    publishSensorDiscovery("Voice Command", "none", "");
    publishSensorDiscovery("WiFi Signal", "signal_strength", "dBm");
    publishSensorDiscovery("Uptime", "none", "s");
    publishSensorDiscovery("Free Memory", "none", "KB");
    
    // Publish binary sensors
    publishBinarySensorDiscovery("Voice Active", "sound");
    publishBinarySensorDiscovery("MQTT Connected", "connectivity");
    
    // Publish switches (for scenes)
    publishSwitchDiscovery("Welcome Home Scene");
    publishSwitchDiscovery("Good Night Scene");
    publishSwitchDiscovery("Away Mode Scene");
    
    discoveryPublished = true;
    Serial.println("Discovery published");
}

void HomeAssistantIntegration::publishSensorDiscovery(const char* name, const char* deviceClass, const char* unit) {
    JsonDocument doc;
    
    String objectId = String(name);
    objectId.toLowerCase();
    objectId.replace(" ", "_");
    
    doc["name"] = name;
    doc["unique_id"] = getUniqueId(objectId.c_str());
    doc["state_topic"] = String("entryhub/sensor/") + objectId;
    
    if (strlen(deviceClass) > 0) {
        doc["device_class"] = deviceClass;
    }
    
    if (strlen(unit) > 0) {
        doc["unit_of_measurement"] = unit;
    }
    
    // Device information
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = DEVICE_NAME;
    device["name"] = DEVICE_NAME;
    device["model"] = "ESP32-S3 Entry Hub";
    device["manufacturer"] = "Custom";
    device["sw_version"] = DEVICE_VERSION;
    
    String topic = String(HA_DISCOVERY_PREFIX) + "/sensor/" + DEVICE_NAME + "/" + objectId + "/config";
    mqttClient.publishJson(topic.c_str(), doc, true);
}

void HomeAssistantIntegration::publishBinarySensorDiscovery(const char* name, const char* deviceClass) {
    JsonDocument doc;
    
    String objectId = String(name);
    objectId.toLowerCase();
    objectId.replace(" ", "_");
    
    doc["name"] = name;
    doc["unique_id"] = getUniqueId(objectId.c_str());
    doc["state_topic"] = String("entryhub/binary_sensor/") + objectId;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    
    if (strlen(deviceClass) > 0) {
        doc["device_class"] = deviceClass;
    }
    
    // Device information
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = DEVICE_NAME;
    device["name"] = DEVICE_NAME;
    device["model"] = "ESP32-S3 Entry Hub";
    device["manufacturer"] = "Custom";
    
    String topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" + DEVICE_NAME + "/" + objectId + "/config";
    mqttClient.publishJson(topic.c_str(), doc, true);
}

void HomeAssistantIntegration::publishSwitchDiscovery(const char* name) {
    JsonDocument doc;
    
    String objectId = String(name);
    objectId.toLowerCase();
    objectId.replace(" ", "_");
    
    doc["name"] = name;
    doc["unique_id"] = getUniqueId(objectId.c_str());
    doc["state_topic"] = String("entryhub/switch/") + objectId + "/state";
    doc["command_topic"] = String("entryhub/switch/") + objectId + "/set";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    
    // Device information
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = DEVICE_NAME;
    device["name"] = DEVICE_NAME;
    
    String topic = String(HA_DISCOVERY_PREFIX) + "/switch/" + DEVICE_NAME + "/" + objectId + "/config";
    mqttClient.publishJson(topic.c_str(), doc, true);
}

void HomeAssistantIntegration::controlLight(const char* entityId, bool state, int brightness) {
    String topic = String("homeassistant/light/") + entityId + "/set";
    
    JsonDocument doc;
    doc["state"] = state ? "ON" : "OFF";
    
    if (brightness >= 0) {
        doc["brightness"] = brightness;
    }
    
    mqttClient.publishJson(topic.c_str(), doc);
}

void HomeAssistantIntegration::controlSwitch(const char* entityId, bool state) {
    String topic = String("homeassistant/switch/") + entityId + "/set";
    mqttClient.publish(topic.c_str(), state ? "ON" : "OFF");
}

void HomeAssistantIntegration::controlLock(const char* entityId, bool locked) {
    String topic = String("homeassistant/lock/") + entityId + "/set";
    mqttClient.publish(topic.c_str(), locked ? "LOCK" : "UNLOCK");
}

void HomeAssistantIntegration::controlCover(const char* entityId, const char* action) {
    String topic = String("homeassistant/cover/") + entityId + "/set";
    mqttClient.publish(topic.c_str(), action);
}

void HomeAssistantIntegration::updatePresenceSensor(const char* person, bool present) {
    String objectId = String(person);
    objectId.toLowerCase();
    
    String topic = String("entryhub/binary_sensor/presence_") + objectId;
    mqttClient.publish(topic.c_str(), present ? "ON" : "OFF");
}

void HomeAssistantIntegration::updateVoiceCommandSensor(const char* command) {
    mqttClient.publish("entryhub/sensor/voice_command", command);
}

void HomeAssistantIntegration::activateScene(const char* sceneId) {
    String topic = String("homeassistant/scene/") + sceneId + "/set";
    mqttClient.publish(topic.c_str(), "ON");
}

String HomeAssistantIntegration::getUniqueId(const char* component) {
    return String(DEVICE_NAME) + "_" + component;
}

String HomeAssistantIntegration::getDeviceConfig() {
    JsonDocument doc;
    
    doc["identifiers"][0] = DEVICE_NAME;
    doc["name"] = DEVICE_NAME;
    doc["model"] = "ESP32-S3 Entry Hub";
    doc["manufacturer"] = "Custom";
    doc["sw_version"] = DEVICE_VERSION;
    
    String output;
    serializeJson(doc, output);
    return output;
}

void HomeAssistantIntegration::publishDeviceState() {
    // Publish sensor states
    mqttClient.publish("entryhub/sensor/wifi_signal", String(WiFi.RSSI()).c_str());
    mqttClient.publish("entryhub/sensor/uptime", String(millis() / 1000).c_str());
    mqttClient.publish("entryhub/sensor/free_memory", String(ESP.getFreeHeap() / 1024).c_str());
    
    // Publish binary sensor states
    mqttClient.publish("entryhub/binary_sensor/mqtt_connected", mqttClient.isConnected() ? "ON" : "OFF");
}

void HomeAssistantIntegration::handleEntityState(const char* topic, const char* payload) {
    // Handle incoming entity state updates
    Serial.printf("HA Entity State: %s = %s\n", topic, payload);
}
