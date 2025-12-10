#include "storage_manager.h"
#include "config.h"

StorageManager storage;

StorageManager::StorageManager() : initialized(false) {
}

bool StorageManager::begin() {
    Serial.println("Initializing LittleFS...");
    
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return false;
    }
    
    initialized = true;
    Serial.println("LittleFS mounted successfully");
    
    // Print filesystem info
    Serial.printf("Total space: %u bytes\n", getTotalSpace());
    Serial.printf("Used space: %u bytes\n", getUsedSpace());
    
    return true;
}

bool StorageManager::loadConfig(JsonDocument& doc) {
    return readJsonFile(CONFIG_FILE, doc);
}

bool StorageManager::saveConfig(const JsonDocument& doc) {
    return writeJsonFile(CONFIG_FILE, doc);
}

bool StorageManager::loadCommands(JsonDocument& doc) {
    return readJsonFile(COMMANDS_FILE, doc);
}

bool StorageManager::saveCommands(const JsonDocument& doc) {
    return writeJsonFile(COMMANDS_FILE, doc);
}

bool StorageManager::loadPresence(JsonDocument& doc) {
    return readJsonFile(PRESENCE_FILE, doc);
}

bool StorageManager::savePresence(const JsonDocument& doc) {
    return writeJsonFile(PRESENCE_FILE, doc);
}

bool StorageManager::readFile(const char* path, String& content) {
    if (!initialized) {
        return false;
    }
    
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("Failed to open file for reading: %s\n", path);
        return false;
    }
    
    content = file.readString();
    file.close();
    return true;
}

bool StorageManager::writeFile(const char* path, const String& content) {
    if (!initialized) {
        return false;
    }
    
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.printf("Failed to open file for writing: %s\n", path);
        return false;
    }
    
    size_t bytesWritten = file.print(content);
    file.close();
    
    return bytesWritten > 0;
}

bool StorageManager::deleteFile(const char* path) {
    if (!initialized) {
        return false;
    }
    
    return LittleFS.remove(path);
}

bool StorageManager::fileExists(const char* path) {
    if (!initialized) {
        return false;
    }
    
    return LittleFS.exists(path);
}

void StorageManager::listFiles() {
    if (!initialized) {
        return;
    }
    
    Serial.println("Files in LittleFS:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    
    while (file) {
        Serial.printf("  %s (%u bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

size_t StorageManager::getTotalSpace() {
    return LittleFS.totalBytes();
}

size_t StorageManager::getUsedSpace() {
    return LittleFS.usedBytes();
}

bool StorageManager::readJsonFile(const char* path, JsonDocument& doc) {
    String content;
    if (!readFile(path, content)) {
        return false;
    }
    
    DeserializationError error = deserializeJson(doc, content);
    if (error) {
        Serial.printf("Failed to parse JSON from %s: %s\n", path, error.c_str());
        return false;
    }
    
    return true;
}

bool StorageManager::writeJsonFile(const char* path, const JsonDocument& doc) {
    String output;
    serializeJson(doc, output);
    return writeFile(path, output);
}
