#ifndef HA_INTEGRATION_H
#define HA_INTEGRATION_H

#include <ArduinoJson.h>
#include "mqtt_client.h"

class HomeAssistantIntegration {
public:
    HomeAssistantIntegration();
    void begin();
    void loop();
    
    // MQTT Discovery
    void publishDiscovery();
    void publishSensorDiscovery(const char* name, const char* deviceClass, const char* unit);
    void publishBinarySensorDiscovery(const char* name, const char* deviceClass);
    void publishSwitchDiscovery(const char* name);
    
    // Entity control
    void controlLight(const char* entityId, bool state, int brightness = -1);
    void controlSwitch(const char* entityId, bool state);
    void controlLock(const char* entityId, bool locked);
    void controlCover(const char* entityId, const char* action); // open, close, stop
    
    // State updates
    void updatePresenceSensor(const char* person, bool present);
    void updateVoiceCommandSensor(const char* command);
    
    // Scenes
    void activateScene(const char* sceneId);
    
private:
    unsigned long lastUpdate;
    bool discoveryPublished;
    
    String getDeviceConfig();
    String getUniqueId(const char* component);
    void publishDeviceState();
    
    // Entity state callbacks
    void handleEntityState(const char* topic, const char* payload);
};

extern HomeAssistantIntegration homeAssistant;

#endif
