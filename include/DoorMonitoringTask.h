#ifndef DOORMONITORINGTASK_H
#define DOORMONITORINGTASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ApiClient.h"
#include "RelayControl.h"
#include "DoorSensor.h"
#include "BuzzerControl.h"
#include "LCDDisplay.h"
#include "config.h"

class DoorMonitoringTask {
public:
    DoorMonitoringTask(ApiClient& api, RelayControl& relay, DoorSensor& door, 
                       BuzzerControl& buzzer, LCDDisplay& lcd, const String& doorId);
    void begin();
    void stop();
    
    void notifyAccessGranted();
    void notifyAccessRevoked();
    bool isAccessGranted() const;
    
private:
    ApiClient& api;
    RelayControl& relay;
    DoorSensor& door;
    BuzzerControl& buzzer;
    LCDDisplay& lcd;
    String doorId;
    
    TaskHandle_t taskHandle;
    bool running;
    bool lastReportedLockState;
    unsigned long lockStateChangeTime;
    unsigned long lastReportTime;
    
    bool accessGranted;
    unsigned long unlockTime;
    unsigned long doorCloseTime;
    bool lastDoorOpen;
    bool alarmTriggered;
    
    static void monitoringTaskFunction(void* param);
    void monitorLoop();
    bool reportStatus();
    void checkAlarms();
    void handleRelockLogic();
};

#endif
