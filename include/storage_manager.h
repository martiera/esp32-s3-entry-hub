#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <LittleFS.h>
#include <ArduinoJson.h>

class StorageManager {
public:
    StorageManager();
    bool begin();
    
    // Configuration management
    bool loadConfig(JsonDocument& doc);
    bool saveConfig(const JsonDocument& doc);
    
    // Commands management
    bool loadCommands(JsonDocument& doc);
    bool saveCommands(const JsonDocument& doc);
    
    // Presence data
    bool loadPresence(JsonDocument& doc);
    bool savePresence(const JsonDocument& doc);
    
    // Generic file operations
    bool readFile(const char* path, String& content);
    bool writeFile(const char* path, const String& content);
    bool deleteFile(const char* path);
    bool fileExists(const char* path);
    
    // Utility
    void listFiles();
    size_t getTotalSpace();
    size_t getUsedSpace();
    
private:
    bool initialized;
    bool readJsonFile(const char* path, JsonDocument& doc);
    bool writeJsonFile(const char* path, const JsonDocument& doc);
};

extern StorageManager storage;

#endif
