#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ArduinoOTA.h>

class OTAManager {
public:
    OTAManager();
    void begin();
    void loop();
    
private:
    void setupArduinoOTA();
    
    static void onOTAStart();
    static void onOTAEnd();
    static void onOTAProgress(unsigned int progress, unsigned int total);
    static void onOTAError(ota_error_t error);
};

extern OTAManager otaManager;

#endif
