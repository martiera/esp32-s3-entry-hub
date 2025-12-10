#include "wifi_manager.h"
#include "config.h"

WiFiConnectionManager wifiMgr;

WiFiConnectionManager::WiFiConnectionManager() 
    : lastReconnectAttempt(0), shouldSaveConfig(false) {
}

void WiFiConnectionManager::begin() {
    Serial.println("Starting WiFi Manager...");
    
    // Set custom parameters for WiFiManager portal
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
        Serial.println("Entered config mode");
        Serial.print("AP SSID: ");
        Serial.println(myWiFiManager->getConfigPortalSSID());
    });
    
    wifiManager.setSaveConfigCallback([this]() {
        Serial.println("Should save config");
        this->shouldSaveConfig = true;
    });
    
    // Timeout for config portal
    wifiManager.setConfigPortalTimeout(180);
    
    // Set minimum signal quality
    wifiManager.setMinimumSignalQuality(20);
    
    // Auto connect or start config portal
    String apName = String(DEVICE_NAME) + "-Setup";
    
    if (!wifiManager.autoConnect(apName.c_str())) {
        Serial.println("Failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
    }
    
    Serial.println("WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Setup mDNS
    if (ENABLE_MDNS) {
        if (MDNS.begin(HOSTNAME)) {
            Serial.print("mDNS responder started: ");
            Serial.print(HOSTNAME);
            Serial.println(".local");
            MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        }
    }
}

void WiFiConnectionManager::loop() {
    handleReconnection();
}

bool WiFiConnectionManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiConnectionManager::getIPAddress() {
    return WiFi.localIP().toString();
}

String WiFiConnectionManager::getSSID() {
    return WiFi.SSID();
}

int WiFiConnectionManager::getRSSI() {
    return WiFi.RSSI();
}

void WiFiConnectionManager::resetSettings() {
    wifiManager.resetSettings();
    Serial.println("WiFi settings reset");
}

void WiFiConnectionManager::handleReconnection() {
    if (!isConnected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            Serial.println("WiFi disconnected. Attempting to reconnect...");
            WiFi.reconnect();
        }
    }
}
