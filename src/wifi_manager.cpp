#include "wifi_manager.h"
#include "config.h"
#include "notification_manager.h"

WiFiConnectionManager wifiMgr;

WiFiConnectionManager::WiFiConnectionManager() 
    : lastReconnectAttempt(0), shouldSaveConfig(false), lastWiFiState(false), reconnectFailures(0) {
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
    bool currentState = isConnected();
    
    // Detect state change
    if (currentState != lastWiFiState) {
        if (currentState) {
            // Just reconnected
            Serial.println("WiFi reconnected!");
            Serial.printf("  SSID: %s, RSSI: %d dBm, IP: %s\n", 
                         WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
            reconnectFailures = 0;  // Reset counter on successful connection
            notificationManager.notifyConnectionIssue("WiFi", true);
        } else {
            // Just disconnected - log reason
            wl_status_t status = WiFi.status();
            Serial.printf("WiFi disconnected! Status: %d (", status);
            switch(status) {
                case WL_NO_SSID_AVAIL: Serial.print("SSID not found"); break;
                case WL_CONNECT_FAILED: Serial.print("Connection failed"); break;
                case WL_CONNECTION_LOST: Serial.print("Connection lost"); break;
                case WL_DISCONNECTED: Serial.print("Disconnected"); break;
                default: Serial.print("Unknown"); break;
            }
            Serial.println(")");
            notificationManager.notifyConnectionIssue("WiFi", false);
        }
        lastWiFiState = currentState;
    }
    
    if (!isConnected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            reconnectFailures++;
            
            Serial.printf("WiFi reconnect attempt #%d (max: %d)...\n", 
                         reconnectFailures, WIFI_MAX_RECONNECT_FAILURES);
            
            // Reboot after too many failures
            if (reconnectFailures >= WIFI_MAX_RECONNECT_FAILURES) {
                Serial.println("\n⚠️  Too many WiFi reconnect failures. Rebooting in 5 seconds...");
                Serial.println("Possible causes:");
                Serial.println("  - WiFi password changed");
                Serial.println("  - Router is down/restarting");
                Serial.println("  - Signal too weak");
                Serial.println("  - Router DHCP pool exhausted\n");
                delay(5000);
                ESP.restart();
            }
            
            WiFi.reconnect();
        }
    } else {
        // Connected - reset failure counter
        reconnectFailures = 0;
    }
}
