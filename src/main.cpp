/******************************************************
 * Đồ án môn học: Hệ thống điểm danh/mở cửa bằng thẻ từ ESP32
 * 
 * Các chức năng chính:
 * - Đăng ký thiết bị lên server
 * - Quẹt thẻ NFC để mở cửa
 * - Kiểm tra quyền truy cập (Online/Offline)
 * - Tự động đồng bộ cấu hình
 * - Báo cáo trạng thái về server
 * 
 * Xem file config.h để chỉnh các thông số
 ******************************************************/

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

// Cấu hình
#include "config.h"

// Các thư viện module tự cấu hình
#include "Models.h"
#include "WiFiManager.h"
#include "ApiClient.h"
#include "NFCReader.h"
#include "LCDDisplay.h"
#include "RelayControl.h"
#include "BuzzerControl.h"
#include "DoorSensor.h"
#include "ButtonControl.h"
#include "AccessController.h"
#include "CommandPollingTask.h"
#include "DoorMonitoringTask.h"
#include "ConfigManager.h"
#include "ConfigPortal.h"

// Quản lý cấu hình
ConfigManager configManager;
DeviceConfiguration deviceConfig;

// Khởi tạo các module (sẽ cài đặt thông số sau khi load config)
WiFiManager wifiManager(WIFI_SSID, WIFI_PASSWORD);
ApiClient apiClient(API_BASE_URL);  // Link API sẽ cập nhật đè lại sau
NFCReader nfcReader(PIN_NFC_SS, PIN_NFC_RST);
LCDDisplay lcdDisplay(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
RelayControl relayControl(PIN_RELAY_CH2, RELAY_ACTIVE_LOW);
BuzzerControl buzzer(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
DoorSensor doorSensor(PIN_DOOR_SENSOR, DOOR_OPEN_LEVEL);
ButtonControl button(PIN_BUTTON, BUTTON_ACTIVE_LOW);

// Trang web cấu hình
ConfigPortal configPortal(configManager);

#if ENABLE_STATUS_REPORTING
DoorMonitoringTask monitoringTask(apiClient, relayControl, doorSensor, buzzer, lcdDisplay, DOOR_ID);
#endif

// Bộ điều khiển ra vào
#if ENABLE_STATUS_REPORTING
AccessController accessController(nfcReader, apiClient, relayControl, lcdDisplay, buzzer, doorSensor, monitoringTask);
#else
#error "STATUS_REPORTING must be enabled for proper door monitoring"
#endif

#if ENABLE_COMMAND_POLLING
CommandPollingTask pollingTask(apiClient, accessController, lcdDisplay, relayControl);
#endif

bool inConfigMode = false;
String deviceToken;
uint32_t lastHeartbeat = 0;
uint32_t lastConfigRefresh = 0;
uint32_t bootTime = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n==========================================");
    Serial.println("Do an IoT - He thong cua tu dong");
    Serial.println("Firmware: " FIRMWARE_VERSION);
    Serial.println("==========================================\n");
    
    bootTime = millis();
    
    Serial.println("[INIT] Dang khoi tao phan cung...");
    
    lcdDisplay.begin(PIN_LCD_SDA, PIN_LCD_SCL);
    lcdDisplay.show("Dang khoi dong", "Vui long cho...");
    
    SPI.begin(PIN_NFC_SCK, PIN_NFC_MISO, PIN_NFC_MOSI, PIN_NFC_SS);
    nfcReader.begin();
    
    relayControl.begin();
    buzzer.begin();
    doorSensor.begin();
    button.begin();
    
    Serial.println("[INIT] Phan cung OK");
    
    Serial.println("[INIT] Dang doc cau hinh...");
    configManager.begin();
    
    bool configLoaded = configManager.load(deviceConfig);
    
   // Nếu là lần đầu chạy (chưa có config) thì vào chế độ cài đặt
    if (!configLoaded || !deviceConfig.configured) {
        Serial.println("[INIT] Phat hien chay lan dau - Vao che do cau hinh");
        
        buzzer.toneTimed(2000, 150);
        delay(200);
        buzzer.toneTimed(2000, 150);
        delay(200);
        buzzer.toneTimed(2000, 150);
        delay(500);
        
        lcdDisplay.show("Che do cai dat", "Cau hinh lan dau");
        delay(2000);
        
        configPortal.begin();
        lcdDisplay.show("Wifi: Setup Mode", configPortal.getAPSSID().c_str());
        
        while (true) {
            configPortal.update();
            delay(10);
        }
    }
    
    Serial.println("[INIT] Load cau hinh thanh cong");
    Serial.print("[INIT] Device ID: ");
    Serial.println(deviceConfig.device_id);
    Serial.print("[INIT] WiFi SSID: ");
    Serial.println(deviceConfig.wifi_ssid);
    Serial.print("[INIT] API URL: ");
    Serial.println(deviceConfig.api_base_url);
    
    // Cập nhật lại thông số cho các module
    wifiManager.setCredentials(deviceConfig.wifi_ssid, deviceConfig.wifi_password);
    apiClient.setBaseUrl(deviceConfig.api_base_url);
    
    // Kết nối Wifi
    lcdDisplay.show("Dang ket noi...", deviceConfig.wifi_ssid);
    wifiManager.begin();
    
    if (!wifiManager.connect()) {
        Serial.println("[INIT] Ket noi Wifi that bai -> Vao che do Config");
        
        buzzer.toneTimed(2000, 150);
        delay(200);
        buzzer.toneTimed(2000, 150);
        delay(200);
        buzzer.toneTimed(2000, 150);
        delay(500);
        
        lcdDisplay.show("Loi Wifi", "Vao cau hinh...");
        delay(2000);
        
        configPortal.begin();
        lcdDisplay.show("Wifi: Setup Mode", configPortal.getAPSSID().c_str());
        
        while (true) {
            configPortal.update();
            delay(10);
        }
    }
    
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[INIT] Doi dong bo thoi gian...");
    delay(2000);
    
    lcdDisplay.show("Dang dang ky...", "Ket noi Server");
    Serial.println("[INIT] Dang dang ky voi server...");
    String deviceToken;
    if (apiClient.registerDevice(deviceToken)) {
      apiClient.setDeviceToken(deviceToken);
      Serial.println("[INIT] Dang ky thanh cong");
    } else {
      Serial.println("[INIT] Dang ky that bai - se thu lai sau");
      lcdDisplay.show("Loi ket noi", "Thu lai sau...");
    }
    
    // Tải cấu hình chi tiết từ server (nếu có token)
    DeviceConfig config;
    if (deviceToken.length() > 0 && apiClient.getConfig(config)) {
      Serial.println("[INIT] Da tai duoc cau hinh tu server");
      Serial.print("[INIT] Thoi gian mo cua: ");
      Serial.print(config.relay_open_ms);
      Serial.println("ms");
      
      accessController.updateConfig(config);
    } else if (deviceToken.length() > 0) {
      Serial.println("[INIT] Khong tai duoc cau hinh");
    }
    
    lcdDisplay.show("San sang", "Moi quet the");
    buzzer.accessGranted();  // Kêu cái bíp báo hiệu xong
    Serial.println("[INIT] He thong san sang!");
    
    #if ENABLE_COMMAND_POLLING
    Serial.println("[INIT] Bat tac vu nhan lenh...");
    pollingTask.begin();
    #endif
    
    #if ENABLE_STATUS_REPORTING
    Serial.println("[INIT] Bat tac vu theo doi cua...");
    monitoringTask.begin();
    #endif
    
}


void loop() {
    if (inConfigMode) {
        configPortal.update();
        
        static unsigned long lastButtonCheck = 0;
        unsigned long now = millis();
        if (now - lastButtonCheck > 100) {
            bool buttonPressed = button.wasPressed();
            if (buttonPressed) {
                Serial.println("[CONFIG] Nhan nut -> Thoat config, khoi dong lai");
                lcdDisplay.show("Thoat Config", "Restarting...");
                delay(1000);
                ESP.restart();
            }
            lastButtonCheck = now;
        }
        
        delay(10);
        return;
    }
    
    static unsigned long lastHeartbeat = 0;
    static unsigned long lastConfigRefresh = 0;
    static unsigned long lastRegistrationRetry = 0;
    static unsigned long lastHealthCheck = 0;
    
    unsigned long now = millis();
    
    // Kiểm tra Wifi, mất thì kết nối lại
    if (!wifiManager.isConnected()) {
        wifiManager.reconnect();
    }
    
    if (now - lastHealthCheck >= 30000 || lastHealthCheck == 0) {
        lastHealthCheck = now;
        
        bool wasOffline = apiClient.isOffline();
        bool healthy = apiClient.checkHealth();
        
        if (wasOffline && healthy) {
            Serial.println("[HEALTH] Có mạng lại rồi - chuyen sang Online!");
            lcdDisplay.show("Online", "Ready");
            delay(1000);
            lcdDisplay.show("San sang", "Moi quet the");
            
            String currentToken;
            apiClient.getDeviceToken(currentToken);
            if (currentToken.length() == 0) {
                Serial.println("[HEALTH] Dang ky lai thiet bi...");
                String newToken;
                if (apiClient.registerDevice(newToken)) {
                    apiClient.setDeviceToken(newToken);
                    
                    DeviceConfig config;
                    if (apiClient.getConfig(config)) {
                        accessController.updateConfig(config);
                    }
                }
            }
        } else if (!wasOffline && !healthy) {
            Serial.println("[HEALTH] Mat ket noi server - chuyen sang Offline");
        }
    }
    
    if (apiClient.getFailureCount() < MAX_API_FAILURES) {
      String currentToken;
      apiClient.getDeviceToken(currentToken);
      
      if (currentToken.length() == 0 && (now - lastRegistrationRetry >= 30000 || lastRegistrationRetry == 0)) {
        lastRegistrationRetry = now;
        Serial.println("[RETRY] Thu dang ky lai...");
        lcdDisplay.show("Dang ket noi...", "Vui long cho");
        
        String newToken;
        if (apiClient.registerDevice(newToken)) {
          apiClient.setDeviceToken(newToken);
          Serial.println("[RETRY] Dang ky thanh cong!");
          lcdDisplay.show("San sang", "Moi quet the");
          
          DeviceConfig config;
          if (apiClient.getConfig(config)) {
            Serial.println("[RETRY] Da lay config");
            accessController.updateConfig(config);
          } else {
            Serial.println("[RETRY] Lay config that bai");
          }
        } else {
          Serial.println("[RETRY] Van that bai, doi ti thu lai.");
          lcdDisplay.show("Loi ket noi", "Thu lai sau...");
        }
      }
    }
    
    // Xử lý nút nhấn
    // Nhấn nhả: Mở cửa
    // Nhấn giữ 3s: Vào chế độ cài đặt
    static unsigned long buttonPressStart = 0;
    static bool buttonWasPressed = false;
    
    int buttonState = digitalRead(PIN_BUTTON);
    bool buttonPressed = (BUTTON_ACTIVE_LOW) ? (buttonState == LOW) : (buttonState == HIGH);
    
    if (buttonPressed && !buttonWasPressed) {
        buttonPressStart = now;
        buttonWasPressed = true;
    } else if (buttonPressed && buttonWasPressed) {
        if ((now - buttonPressStart) >= CONFIG_BUTTON_HOLD_MS) {
            Serial.println("[CONFIG] Phat hien nhan giu -> Vao Config Mode");
            
            inConfigMode = true;
            
            buzzer.toneTimed(2000, 150);
            delay(200);
            buzzer.toneTimed(2000, 150);
            delay(200);
            buzzer.toneTimed(2000, 150);
            delay(200);
            
            lcdDisplay.show("Config Mode", "Starting...");
            delay(1000);
            
            configPortal.begin();
            lcdDisplay.show("Config Mode", configPortal.getAPSSID().c_str());
            
            buttonWasPressed = false;
        }
    } else if (!buttonPressed && buttonWasPressed) {
        unsigned long pressDuration = now - buttonPressStart;
        if (pressDuration < CONFIG_BUTTON_HOLD_MS && pressDuration > BUTTON_DEBOUNCE_MS) {
            Serial.println("[BUTTON] Mo cua bang nut cung");
            accessController.grantAccess("BUTTON_PRESS");
            lcdDisplay.show("Nut nhan", "Mo cua");
        }
        buttonWasPressed = false;
    }
    
    if (nfcReader.isCardPresent()) {
        accessController.handleCardTap();
    }
    
    accessController.update();
    
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        
        DeviceStatus status;
        status.uptime_sec = (now - bootTime) / 1000;
        status.rssi = wifiManager.getRSSI(); // Sóng wifi khỏe không
        status.fw_version = FIRMWARE_VERSION;
        status.last_access_ts = ""; 
        
        if (apiClient.sendHeartbeat(status)) {
            Serial.println("[HEARTBEAT] Gui ok");
        } else {
            Serial.println("[HEARTBEAT] Gui that bai");
        }
        
        if (accessController.getQueuedLogCount() > 0) {
            accessController.uploadQueuedLogs();
        }
    }
    
    if (now - lastConfigRefresh >= CONFIG_REFRESH_INTERVAL_MS) {
        lastConfigRefresh = now;
        
        DeviceConfig config;
        if (apiClient.getConfig(config)) {
            Serial.println("[CONFIG] Cap nhat config ok");
            accessController.updateConfig(config);
        } else {
            Serial.println("[CONFIG] Cap nhat config loi");
        }
    }
    
    delay(10);
}
