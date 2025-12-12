#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <Arduino.h>
#include "led_feedback.h"
#include <ArduinoJson.h>

// Notification types
enum class NotificationType {
    CALENDAR_REMINDER,      // Upcoming calendar event
    PRESENCE_CHANGE,        // Someone arrived/left
    WEATHER_ALERT,          // Severe weather warning
    CONNECTION_ISSUE,       // WiFi/MQTT/HA connection problem
    SYSTEM_UPDATE,          // Update available
    CONFIGURATION_NEEDED,   // Setup required
    DOOR_EVENT,             // Door sensor triggered
    CUSTOM                  // Custom notification via MQTT/API
};

// Notification priority levels
enum class NotificationPriority {
    PRIORITY_LOW,        // Informational, gentle notification
    PRIORITY_NORMAL,     // Standard notification
    PRIORITY_HIGH,       // Important, requires attention
    PRIORITY_URGENT      // Critical, persistent until acknowledged
};

// Notification configuration for each type
struct NotificationConfig {
    bool enabled;
    LedPattern pattern;
    LedColor color;
    uint16_t duration;      // milliseconds (0 = until acknowledged)
    uint8_t repeatCount;    // 0 = infinite until acknowledged
};

class NotificationManager {
public:
    NotificationManager();
    void begin();
    void loop();
    
    // Trigger notifications
    void notifyCalendarReminder(const String& eventName, int minutesUntil);
    void notifyPresenceChange(const String& person, bool arrived);
    void notifyWeatherAlert(const String& alert);
    void notifyConnectionIssue(const String& service, bool resolved);
    void notifySystemUpdate();
    void notifyConfigurationNeeded();
    void notifyDoorEvent();
    void notifyCustom(const String& message, LedColor color, LedPattern pattern);
    
    // Control
    void acknowledge();                    // Clear current notification
    void acknowledgeAll();                 // Clear all notifications
    bool hasActiveNotification();
    String getCurrentNotification();
    
    // Configuration
    void loadConfig();
    void setNotificationEnabled(NotificationType type, bool enabled);
    void setNotificationPattern(NotificationType type, LedPattern pattern, LedColor color, uint16_t duration);
    bool isNotificationEnabled(NotificationType type);
    
    // Get configuration for web UI
    void getConfigJson(JsonDocument& doc);
    
private:
    struct ActiveNotification {
        NotificationType type;
        NotificationPriority priority;
        String message;
        unsigned long startTime;
        uint16_t duration;
        uint8_t repeatCount;
        uint8_t currentRepeat;
        bool active;
    };
    
    NotificationConfig configs[8];  // One for each NotificationType
    ActiveNotification currentNotification;
    bool notificationsEnabled;
    
    void triggerNotification(NotificationType type, NotificationPriority priority, const String& message);
    void updateLedForNotification();
    NotificationConfig& getConfig(NotificationType type);
};

extern NotificationManager notificationManager;

#endif
