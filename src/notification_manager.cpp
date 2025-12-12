#include "notification_manager.h"
#include "storage_manager.h"
#include "config.h"

NotificationManager notificationManager;

NotificationManager::NotificationManager()
    : notificationsEnabled(true) {
    // Initialize with default configurations
    configs[0] = {true, LedPattern::PULSE, LedColor::Blue(), 5000, 3};      // Calendar reminder
    configs[1] = {true, LedPattern::BLINK_SLOW, LedColor::Green(), 3000, 2}; // Presence change
    configs[2] = {true, LedPattern::BLINK_FAST, LedColor::Red(), 10000, 0}; // Weather alert (continuous)
    configs[3] = {true, LedPattern::BLINK_SLOW, LedColor::Yellow(), 5000, 3}; // Connection issue
    configs[4] = {true, LedPattern::PULSE, LedColor::Cyan(), 5000, 2};      // System update
    configs[5] = {true, LedPattern::BLINK_SLOW, LedColor::Orange(), 0, 0};  // Configuration needed (persistent)
    configs[6] = {true, LedPattern::BLINK_FAST, LedColor::White(), 2000, 1}; // Door event
    configs[7] = {true, LedPattern::SOLID, LedColor::Purple(), 3000, 1};    // Custom
    
    currentNotification.active = false;
}

void NotificationManager::begin() {
    loadConfig();
    Serial.println("Notification Manager initialized");
}

void NotificationManager::loop() {
    if (!notificationsEnabled || !currentNotification.active) {
        return;
    }
    
    unsigned long now = millis();
    unsigned long elapsed = now - currentNotification.startTime;
    
    // Check if notification duration expired
    if (currentNotification.duration > 0 && elapsed >= currentNotification.duration) {
        if (currentNotification.repeatCount == 0) {
            // Infinite repeat, restart
            currentNotification.startTime = now;
            updateLedForNotification();
        } else if (currentNotification.currentRepeat < currentNotification.repeatCount) {
            // Still have repeats left
            currentNotification.currentRepeat++;
            currentNotification.startTime = now;
            updateLedForNotification();
        } else {
            // Notification complete
            acknowledge();
        }
    }
}

void NotificationManager::notifyCalendarReminder(const String& eventName, int minutesUntil) {
    if (!isNotificationEnabled(NotificationType::CALENDAR_REMINDER)) return;
    
    String message = "Event: " + eventName + " in " + String(minutesUntil) + " min";
    triggerNotification(NotificationType::CALENDAR_REMINDER, NotificationPriority::PRIORITY_NORMAL, message);
}

void NotificationManager::notifyPresenceChange(const String& person, bool arrived) {
    if (!isNotificationEnabled(NotificationType::PRESENCE_CHANGE)) return;
    
    String message = person + (arrived ? " arrived" : " left");
    triggerNotification(NotificationType::PRESENCE_CHANGE, NotificationPriority::PRIORITY_LOW, message);
}

void NotificationManager::notifyWeatherAlert(const String& alert) {
    if (!isNotificationEnabled(NotificationType::WEATHER_ALERT)) return;
    
    triggerNotification(NotificationType::WEATHER_ALERT, NotificationPriority::PRIORITY_HIGH, "Weather Alert: " + alert);
}

void NotificationManager::notifyConnectionIssue(const String& service, bool resolved) {
    if (!isNotificationEnabled(NotificationType::CONNECTION_ISSUE)) return;
    
    String message = service + (resolved ? " connected" : " disconnected");
    triggerNotification(NotificationType::CONNECTION_ISSUE, 
                       resolved ? NotificationPriority::PRIORITY_LOW : NotificationPriority::PRIORITY_NORMAL, 
                       message);
}

void NotificationManager::notifySystemUpdate() {
    if (!isNotificationEnabled(NotificationType::SYSTEM_UPDATE)) return;
    
    triggerNotification(NotificationType::SYSTEM_UPDATE, NotificationPriority::PRIORITY_LOW, "Update available");
}

void NotificationManager::notifyConfigurationNeeded() {
    if (!isNotificationEnabled(NotificationType::CONFIGURATION_NEEDED)) return;
    
    triggerNotification(NotificationType::CONFIGURATION_NEEDED, NotificationPriority::PRIORITY_HIGH, "Configuration needed");
}

void NotificationManager::notifyDoorEvent() {
    if (!isNotificationEnabled(NotificationType::DOOR_EVENT)) return;
    
    triggerNotification(NotificationType::DOOR_EVENT, NotificationPriority::PRIORITY_NORMAL, "Door activity");
}

void NotificationManager::notifyCustom(const String& message, LedColor color, LedPattern pattern) {
    if (!isNotificationEnabled(NotificationType::CUSTOM)) return;
    
    // Temporarily override config for custom notification
    configs[7].color = color;
    configs[7].pattern = pattern;
    triggerNotification(NotificationType::CUSTOM, NotificationPriority::PRIORITY_NORMAL, message);
}

void NotificationManager::triggerNotification(NotificationType type, NotificationPriority priority, const String& message) {
    // If there's an active notification with higher priority, don't override
    if (currentNotification.active && currentNotification.priority > priority) {
        Serial.printf("Notification blocked by higher priority: %s\n", message.c_str());
        return;
    }
    
    NotificationConfig& config = getConfig(type);
    
    currentNotification.type = type;
    currentNotification.priority = priority;
    currentNotification.message = message;
    currentNotification.startTime = millis();
    currentNotification.duration = config.duration;
    currentNotification.repeatCount = config.repeatCount;
    currentNotification.currentRepeat = 1;
    currentNotification.active = true;
    
    Serial.printf("Notification: %s\n", message.c_str());
    
    updateLedForNotification();
}

void NotificationManager::updateLedForNotification() {
    if (!currentNotification.active) return;
    
    NotificationConfig& config = getConfig(currentNotification.type);
    ledFeedback.setPattern(config.pattern, config.color, 500);
}

void NotificationManager::acknowledge() {
    if (currentNotification.active) {
        Serial.printf("Notification acknowledged: %s\n", currentNotification.message.c_str());
        currentNotification.active = false;
        ledFeedback.stopPattern();
        ledFeedback.off();
    }
}

void NotificationManager::acknowledgeAll() {
    acknowledge();
}

bool NotificationManager::hasActiveNotification() {
    return currentNotification.active;
}

String NotificationManager::getCurrentNotification() {
    if (currentNotification.active) {
        return currentNotification.message;
    }
    return "";
}

void NotificationManager::loadConfig() {
    JsonDocument config;
    if (storage.loadConfig(config)) {
        notificationsEnabled = config["notifications"]["enabled"] | true;
        
        // Load individual notification settings
        if (config["notifications"]["calendar_reminder"]["enabled"].is<bool>()) {
            configs[0].enabled = config["notifications"]["calendar_reminder"]["enabled"];
        }
        if (config["notifications"]["presence_change"]["enabled"].is<bool>()) {
            configs[1].enabled = config["notifications"]["presence_change"]["enabled"];
        }
        if (config["notifications"]["weather_alert"]["enabled"].is<bool>()) {
            configs[2].enabled = config["notifications"]["weather_alert"]["enabled"];
        }
        if (config["notifications"]["connection_issue"]["enabled"].is<bool>()) {
            configs[3].enabled = config["notifications"]["connection_issue"]["enabled"];
        }
        if (config["notifications"]["system_update"]["enabled"].is<bool>()) {
            configs[4].enabled = config["notifications"]["system_update"]["enabled"];
        }
        if (config["notifications"]["configuration_needed"]["enabled"].is<bool>()) {
            configs[5].enabled = config["notifications"]["configuration_needed"]["enabled"];
        }
        if (config["notifications"]["door_event"]["enabled"].is<bool>()) {
            configs[6].enabled = config["notifications"]["door_event"]["enabled"];
        }
        if (config["notifications"]["custom"]["enabled"].is<bool>()) {
            configs[7].enabled = config["notifications"]["custom"]["enabled"];
        }
    }
}

void NotificationManager::setNotificationEnabled(NotificationType type, bool enabled) {
    getConfig(type).enabled = enabled;
}

void NotificationManager::setNotificationPattern(NotificationType type, LedPattern pattern, LedColor color, uint16_t duration) {
    NotificationConfig& config = getConfig(type);
    config.pattern = pattern;
    config.color = color;
    config.duration = duration;
}

bool NotificationManager::isNotificationEnabled(NotificationType type) {
    return notificationsEnabled && getConfig(type).enabled;
}

NotificationConfig& NotificationManager::getConfig(NotificationType type) {
    return configs[static_cast<int>(type)];
}

void NotificationManager::getConfigJson(JsonDocument& doc) {
    doc["enabled"] = notificationsEnabled;
    
    const char* typeNames[] = {
        "calendar_reminder",
        "presence_change",
        "weather_alert",
        "connection_issue",
        "system_update",
        "configuration_needed",
        "door_event",
        "custom"
    };
    
    for (int i = 0; i < 8; i++) {
        doc[typeNames[i]]["enabled"] = configs[i].enabled;
        doc[typeNames[i]]["duration"] = configs[i].duration;
    }
}
