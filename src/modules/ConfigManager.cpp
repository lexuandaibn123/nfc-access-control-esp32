#include "ConfigManager.h"
#include "config.h"
#include <string.h>
#include <nvs_flash.h>

const char* ConfigManager::PREF_NAMESPACE = "device_config";
const uint32_t ConfigManager::CONFIG_VERSION = 1;

ConfigManager::ConfigManager() {
}

void ConfigManager::begin() {
    // Initialize NVS (Non-Volatile Storage) - required for Preferences
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    Serial.println("[ConfigManager] NVS initialized");
}

bool ConfigManager::load(DeviceConfiguration& config) {
    if (!preferences.begin(PREF_NAMESPACE, true)) { // Read-only mode
        Serial.println("[ConfigManager] No configuration found, using defaults");
        getDefaults(config);
        return false;
    }
    
    // Check if configured
    bool configured = preferences.getBool("configured", false);
    
    if (!configured) {
        Serial.println("[ConfigManager] Device not configured yet");
        preferences.end();
        getDefaults(config);
        return false;
    }
    
    // Load all values
    preferences.getString("wifi_ssid", config.wifi_ssid, sizeof(config.wifi_ssid));
    preferences.getString("wifi_pass", config.wifi_password, sizeof(config.wifi_password));
    preferences.getString("api_url", config.api_base_url, sizeof(config.api_base_url));
    preferences.getString("device_id", config.device_id, sizeof(config.device_id));
    preferences.getString("device_sec", config.device_secret, sizeof(config.device_secret));
    preferences.getString("door_id", config.door_id, sizeof(config.door_id));
    preferences.getString("hw_type", config.hardware_type, sizeof(config.hardware_type));
    
    config.configured = configured;
    config.config_version = preferences.getUInt("version", CONFIG_VERSION);
    
    preferences.end();
    
    Serial.println("[ConfigManager] Configuration loaded successfully");
    return true;
}

bool ConfigManager::save(const DeviceConfiguration& config) {
    if (!validate(config)) {
        Serial.println("[ConfigManager] Configuration validation failed");
        return false;
    }
    
    if (!preferences.begin(PREF_NAMESPACE, false)) { // Read-write mode
        Serial.println("[ConfigManager] Failed to open preferences for writing");
        return false;
    }
    
    // Save all values
    preferences.putString("wifi_ssid", config.wifi_ssid);
    preferences.putString("wifi_pass", config.wifi_password);
    preferences.putString("api_url", config.api_base_url);
    preferences.putString("device_id", config.device_id);
    preferences.putString("device_sec", config.device_secret);
    preferences.putString("door_id", config.door_id);
    preferences.putString("hw_type", config.hardware_type);
    
    preferences.putBool("configured", true);
    preferences.putUInt("version", CONFIG_VERSION);
    
    preferences.end();
    
    Serial.println("[ConfigManager] Configuration saved successfully");
    return true;
}

bool ConfigManager::isConfigured() {
    if (!preferences.begin(PREF_NAMESPACE, true)) {
        return false;
    }
    
    bool configured = preferences.getBool("configured", false);
    preferences.end();
    
    return configured;
}

void ConfigManager::factoryReset() {
    Serial.println("[ConfigManager] Performing factory reset...");
    
    if (!preferences.begin(PREF_NAMESPACE, false)) {
        Serial.println("[ConfigManager] Failed to open preferences for reset");
        return;
    }
    
    preferences.clear();
    preferences.end();
    
    Serial.println("[ConfigManager] Factory reset complete");
}

void ConfigManager::getDefaults(DeviceConfiguration& config) {
    // WiFi defaults (from config.h)
    strncpy(config.wifi_ssid, WIFI_SSID, sizeof(config.wifi_ssid) - 1);
    strncpy(config.wifi_password, WIFI_PASSWORD, sizeof(config.wifi_password) - 1);
    
    // API defaults
    strncpy(config.api_base_url, API_BASE_URL, sizeof(config.api_base_url) - 1);
    
    // Device defaults
    strncpy(config.device_id, DEVICE_ID, sizeof(config.device_id) - 1);
    strncpy(config.device_secret, DEVICE_SECRET, sizeof(config.device_secret) - 1);
    strncpy(config.door_id, DOOR_ID, sizeof(config.door_id) - 1);
    strncpy(config.hardware_type, HARDWARE_TYPE, sizeof(config.hardware_type) - 1);
    
    config.configured = false;
    config.config_version = CONFIG_VERSION;
}

bool ConfigManager::validate(const DeviceConfiguration& config) {
    // Check WiFi SSID is not empty
    if (strlen(config.wifi_ssid) == 0) {
        Serial.println("[ConfigManager] Validation failed: WiFi SSID is empty");
        return false;
    }
    
    // Check API URL is not empty
    if (strlen(config.api_base_url) == 0) {
        Serial.println("[ConfigManager] Validation failed: API URL is empty");
        return false;
    }
    
    // Check device ID is not empty
    if (strlen(config.device_id) == 0) {
        Serial.println("[ConfigManager] Validation failed: Device ID is empty");
        return false;
    }
    
    // Check door ID is not empty
    if (strlen(config.door_id) == 0) {
        Serial.println("[ConfigManager] Validation failed: Door ID is empty");
        return false;
    }
    
    return true;
}
