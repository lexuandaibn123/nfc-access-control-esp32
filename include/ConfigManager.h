#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// Cấu hình cấu trúc lưu trữ
struct DeviceConfiguration {
    // WiFi Settings
    char wifi_ssid[32];
    char wifi_password[64];
    
    // API Settings
    char api_base_url[128];
    
    // Device Identity
    char device_id[32];
    char device_secret[64];
    char door_id[32];
    char hardware_type[32];
    
    // Metadata
    bool configured;
    uint32_t config_version;
};

class ConfigManager {
public:
    ConfigManager();
    
    // Khởi tạo preferences
    void begin();
    
    // Tải cấu hình từ NVS
    bool load(DeviceConfiguration& config);
    
    // Lưu cấu hình vào NVS
    bool save(const DeviceConfiguration& config);
    
    // Kiểm tra thiết bị đã được cấu hình chưa
    bool isConfigured();
    
    // Reset thiết bị - xóa tất cả cấu hình
    void factoryReset();
    
    // Lấy cấu hình mặc định
    void getDefaults(DeviceConfiguration& config);
    
    // Kiểm tra cấu hình
    bool validate(const DeviceConfiguration& config);
    
private:
    Preferences preferences;
    
    static const char* PREF_NAMESPACE;
    static const uint32_t CONFIG_VERSION;
};

#endif
