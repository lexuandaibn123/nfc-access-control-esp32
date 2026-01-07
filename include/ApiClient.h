#ifndef APICLIENT_H
#define APICLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Models.h"

class ApiClient {
public:
    ApiClient(const char* baseUrl);
    void setDeviceToken(const String& token);
    void getDeviceToken(String& outToken) const;
    void setBaseUrl(const char* newBaseUrl);
    
    // Device APIs
    bool registerDevice(String& outToken);
    bool getConfig(DeviceConfig& outConfig);
    bool sendHeartbeat(const DeviceStatus& status);
    
    // Access APIs
    bool checkAccess(const AccessCheckRequest& request, AccessCheckResponse& outResponse);
    bool createCard(const CardCreateRequest& request, CardCreateResponse& outResponse);
    bool uploadLogs(LogEntry* logs, int count);
    
    // Door Command Polling (Remote Control)
    bool pollDoorCommand(const String& doorId, DoorCommandPollResponse& outResponse);
    bool acknowledgeDoorCommand(const String& doorId, bool success);
    
    // Door Status Reporting
    bool updateDoorStatus(const String& doorId, bool isOpen, bool isOnline);
    
    // Status
    bool isOffline() const;
    int getFailureCount() const;
    bool checkHealth();
    String getTimestamp();
    
private:
    String baseUrl;
    String deviceToken;
    int consecutiveFailures;
    WiFiClientSecure secureClient;
    HTTPClient http;
    
    bool post(const char* endpoint, const JsonDocument& requestDoc, JsonDocument& responseDoc);
    bool get(const char* endpoint, JsonDocument& responseDoc);
    void recordFailure();
    void recordSuccess();
};

#endif

