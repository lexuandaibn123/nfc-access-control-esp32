#include "WiFiManager.h"
#include "config.h"

WiFiManager::WiFiManager(const char* ssid, const char* password)
    : ssid(ssid), password(password), lastReconnectAttempt(0) {
}

void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(DEVICE_ID);
}

bool WiFiManager::connect() {
    Serial.print("[WiFi] Connecting to ");
    Serial.println(ssid);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    
    Serial.println();
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int WiFiManager::getRSSI() {
    if (!isConnected()) return 0;
    return WiFi.RSSI();
}

void WiFiManager::reconnect() {
    uint32_t now = millis();
    
    // Throttle reconnect attempts
    if (now - lastReconnectAttempt < WIFI_RECONNECT_INTERVAL_MS) {
        return;
    }
    
    lastReconnectAttempt = now;
    
    if (!isConnected()) {
        Serial.println("[WiFi] Reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
    }
}

void WiFiManager::setCredentials(const char* newSSID, const char* newPassword) {
    ssid = newSSID;
    password = newPassword;
    Serial.println("[WiFi] Credentials updated");
}
