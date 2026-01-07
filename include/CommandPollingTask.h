#ifndef COMMANDPOLLINGTASK_H
#define COMMANDPOLLINGTASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "ApiClient.h"
#include "AccessController.h"
#include "LCDDisplay.h"
#include "RelayControl.h"
#include "config.h"

class CommandPollingTask {
public:
    CommandPollingTask(ApiClient& api, AccessController& access, 
                       LCDDisplay& lcd, RelayControl& relay);
    
    void begin();
    void stop();
    
    static SemaphoreHandle_t accessMutex;
    
private:
    ApiClient& api;
    AccessController& access;
    LCDDisplay& lcd;
    RelayControl& relay;
    
    TaskHandle_t taskHandle;
    volatile bool running;
    
    static void pollingTaskFunction(void* param);
    void pollLoop();
};

#endif
