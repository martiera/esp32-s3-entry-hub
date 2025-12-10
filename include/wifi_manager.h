#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>

class WiFiConnectionManager {
public:
    WiFiConnectionManager();
    void begin();
    void loop();
    bool isConnected();
    String getIPAddress();
    String getSSID();
    int getRSSI();
    void resetSettings();
    
private:
    WiFiManager wifiManager;
    unsigned long lastReconnectAttempt;
    bool shouldSaveConfig;
    
    void setupAccessPoint();
    void connectToWiFi();
    void handleReconnection();
    static void saveConfigCallback();
};

extern WiFiConnectionManager wifiMgr;

#endif
