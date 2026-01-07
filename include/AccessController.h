#ifndef ACCESSCONTROLLER_H
#define ACCESSCONTROLLER_H

#include "config.h"
#include <Arduino.h>
#include "Models.h"
#include "NFCReader.h"
#include "ApiClient.h"
#include "RelayControl.h"
#include "LCDDisplay.h"
#include "BuzzerControl.h"
#include "DoorSensor.h"

class DoorMonitoringTask;

class AccessController {
public:
    AccessController(NFCReader& nfc, ApiClient& api, RelayControl& relay,
                     LCDDisplay& lcd, BuzzerControl& buzzer, DoorSensor& door,
                     DoorMonitoringTask& doorMonitor);
    
    void update();      
    void handleCardTap(); 
    void grantAccess(const String& reason = "ACCESS_GRANTED"); 
    
    void updateConfig(const DeviceConfig& config);
    
    void queueLog(const LogEntry& log);     
    bool uploadQueuedLogs();                
    
private:
    NFCReader& nfc;
    ApiClient& api;
    RelayControl& relay;
    LCDDisplay& lcd;
    BuzzerControl& buzzer;
    DoorSensor& door;
    DoorMonitoringTask& doorMonitor;
    
    // Danh sách offline (Whitelist)
    OfflineWhitelistItem whitelist[50];
    int whitelistCount;
    String jwtPublicKeyPem;  
    
    LogEntry logQueue[LOG_QUEUE_SIZE];
    int logQueueCount;
    
    void handleBlankCard(const String& card_uid);   
    void handleCardWithId(const CardData& card);    
    bool checkOfflineWhitelist(const String& card_id, const CardData& card); 
    LogEntry createLog(const String& decision, const String& reason, 
                       const String& card_id, const String& card_uid); 
};

#endif
