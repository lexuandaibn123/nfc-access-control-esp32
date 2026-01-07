#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration (MOCK)
// ============================================
#define WIFI_SSID "Hai"
#define WIFI_PASSWORD "29102003"
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RECONNECT_INTERVAL_MS 30000

// ============================================
// Backend API Configuration (MOCK)
// ============================================
#define API_BASE_URL "https://boys-participate-pension-classical.trycloudflare.com/api/v1"
#define API_TIMEOUT_MS 30000

// Device Credentials (MOCK)
#define DEVICE_ID "reader-lobby-01"
#define DEVICE_SECRET "change-this-secret"
#define DOOR_ID "door_main"
#define HARDWARE_TYPE "esp32-rc522"
#define FIRMWARE_VERSION "1.0.0"

// ============================================
// Hardware Pin Configuration
// ============================================
// RC522 NFC Reader (SPI)
#define PIN_NFC_SS   5
#define PIN_NFC_RST  4
#define PIN_NFC_SCK  18
#define PIN_NFC_MISO 19
#define PIN_NFC_MOSI 23

// LCD I2C
#define PIN_LCD_SDA 32
#define PIN_LCD_SCL 33
#define LCD_I2C_ADDR 0x27  // or 0x3F, depends on your module
#define LCD_COLS 16
#define LCD_ROWS 2

// Relay (2-channel module)
#define PIN_RELAY_CH1 25
#define PIN_RELAY_CH2 26  // Used for solenoid/lock
#define RELAY_ACTIVE_LOW false  // Set true if your relay module is LOW-trigger

// Door Sensor (MC-38, NC type)
#define PIN_DOOR_SENSOR 27
#define DOOR_OPEN_LEVEL HIGH  // NC + PULLUP: open = HIGH

// Buzzer
#define PIN_BUZZER 14
#define BUZZER_LEDC_CHANNEL 3

// Button (internal door open)
#define PIN_BUTTON 13
#define BUTTON_ACTIVE_LOW true

// ============================================
// Configuration Portal Settings
// ============================================
#define CONFIG_BUTTON_PIN 13           // Same as door open button
#define CONFIG_BUTTON_HOLD_MS 3000     // Hold for 3 seconds to enter config mode
#define CONFIG_AP_PASSWORD "88888888"   // Default AP password (easy to remember)
#define CONFIG_TIMEOUT_MS 600000        // 10 minutes in config mode before timeout

// ============================================
// Access Control Timing
// ============================================
#define RELOCK_DELAY_MS 3000        // Auto-relock after door closes
#define MAX_UNLOCK_DURATION_MS 15000  // Force lock if held too long
#define CARD_READ_COOLDOWN_MS 1200   // Prevent repeated reads
#define BUTTON_DEBOUNCE_MS 30
#define BUTTON_COOLDOWN_MS 500
#define DOOR_DEBOUNCE_MS 20

// ============================================
// API & Network Settings
// ============================================
#define HEARTBEAT_INTERVAL_MS 300000  // 5 minutes
#define CONFIG_REFRESH_INTERVAL_MS 300000  // 5 minutes (same as heartbeat)
#define MAX_API_FAILURES 3  // Switch to offline mode after 3 consecutive failures
#define LOG_BATCH_SIZE 20  // Max logs to send in one batch
#define LOG_QUEUE_SIZE 100  // Max logs to keep in memory

// ============================================
// Offline Mode Settings
// ============================================
#define OFFLINE_CACHE_FILE "/offline_cache.json"
#define DEVICE_TOKEN_FILE "/device_token.txt"

// ============================================
// Door Command Polling (Remote Control)
// ============================================
#define ENABLE_COMMAND_POLLING true  // Command Polling (Remote Control)
#define COMMAND_POLL_TIMEOUT_MS 35000  // 35s (larger than backend 30s)
#define COMMAND_POLL_RETRY_DELAY_MS 100  // Delay between polls
#define COMMAND_POLL_TASK_STACK_SIZE 8192  // 8KB stack for FreeRTOS task
#define COMMAND_POLL_TASK_PRIORITY 1  // Same priority as loop()

// Door Monitoring & Status Reporting
#define ENABLE_STATUS_REPORTING true
#define DOOR_MONITORING_CHECK_INTERVAL_MS 100  // Check door state every 0.1s

// ============================================
// Buzzer Tones
// ============================================
#define TONE_ACCESS_GRANTED_1 2000
#define TONE_ACCESS_GRANTED_2 2400
#define TONE_ACCESS_DENIED 800
#define TONE_ALARM 2500

#define TONE_DURATION_SHORT 150
#define TONE_DURATION_MEDIUM 300
#define TONE_DURATION_GAP 50

#endif // CONFIG_H
