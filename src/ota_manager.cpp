#include "ota_manager.h"
#include "config.h"

OTAManager otaManager;

OTAManager::OTAManager() {
}

void OTAManager::begin() {
    if (!ENABLE_OTA) {
        return;
    }
    
    setupArduinoOTA();
    
    Serial.println("OTA Update services started");
}

void OTAManager::loop() {
    if (ENABLE_OTA) {
        ArduinoOTA.handle();
    }
}

void OTAManager::setupArduinoOTA() {
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    
    ArduinoOTA.onStart(onOTAStart);
    ArduinoOTA.onEnd(onOTAEnd);
    ArduinoOTA.onProgress(onOTAProgress);
    ArduinoOTA.onError(onOTAError);
    
    ArduinoOTA.begin();
    Serial.println("ArduinoOTA started");
}

void OTAManager::onOTAStart() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "firmware";
    } else {
        type = "filesystem";
    }
    
    Serial.println("OTA Update Started: " + type);
    // Stop other services during OTA
}

void OTAManager::onOTAEnd() {
    Serial.println("\nOTA Update Complete");
}

void OTAManager::onOTAProgress(unsigned int progress, unsigned int total) {
    static unsigned int lastPercent = 0;
    unsigned int percent = (progress / (total / 100));
    
    if (percent != lastPercent && percent % 10 == 0) {
        Serial.printf("OTA Progress: %u%%\n", percent);
        lastPercent = percent;
    }
}

void OTAManager::onOTAError(ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    
    switch (error) {
        case OTA_AUTH_ERROR:
            Serial.println("Auth Failed");
            break;
        case OTA_BEGIN_ERROR:
            Serial.println("Begin Failed");
            break;
        case OTA_CONNECT_ERROR:
            Serial.println("Connect Failed");
            break;
        case OTA_RECEIVE_ERROR:
            Serial.println("Receive Failed");
            break;
        case OTA_END_ERROR:
            Serial.println("End Failed");
            break;
        default:
            Serial.println("Unknown Error");
            break;
    }
}
