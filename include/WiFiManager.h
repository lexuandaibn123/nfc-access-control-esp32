#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
    WiFiManager(const char* ssid, const char* password);
    
    void begin();
    bool connect();
    bool isConnected();
    int getRSSI();
    void reconnect();
    
    void setCredentials(const char* newSSID, const char* newPassword);
    
private:
    String ssid;
    String password;
    uint32_t lastReconnectAttempt;
};

#endif
