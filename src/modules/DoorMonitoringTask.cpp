#include "DoorMonitoringTask.h"

// Reference to global config mode flag from main.cpp
extern bool inConfigMode;

#define DOOR_STATUS_REPORT_INTERVAL_MS 30000  // Report every 30 seconds

DoorMonitoringTask::DoorMonitoringTask(ApiClient& api, RelayControl& relay, 
                                       DoorSensor& door, BuzzerControl& buzzer, 
                                       LCDDisplay& lcd, const String& doorId)
    : api(api), relay(relay), door(door), buzzer(buzzer), lcd(lcd), doorId(doorId), 
      taskHandle(NULL), running(false), lastReportedLockState(false), 
      lockStateChangeTime(0), lastReportTime(0),
      accessGranted(false), unlockTime(0), doorCloseTime(0), 
      lastDoorOpen(false), alarmTriggered(false) {
}

void DoorMonitoringTask::begin() {
    if (taskHandle != NULL) {
        Serial.println("[DOOR_MONITOR] Task already running");
        return;
    }
    
    running = true;
    
    // Create FreeRTOS task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        monitoringTaskFunction,
        "DoorMonitor",
        COMMAND_POLL_TASK_STACK_SIZE,  // Reuse same stack size
        this,
        COMMAND_POLL_TASK_PRIORITY,    // Reuse same priority
        &taskHandle,
        0  // Core 0
    );
    
    if (result == pdPASS) {
        Serial.println("[DOOR_MONITOR] ✅ Task created and started on core 0");
    } else {
        Serial.println("[DOOR_MONITOR] ❌ Failed to create task!");
        running = false;
    }
}

void DoorMonitoringTask::stop() {
    if (taskHandle == NULL) {
        return;
    }
    
    running = false;
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(taskHandle);
    taskHandle = NULL;
    
    Serial.println("[DOOR_MONITOR] Task stopped");
}

void DoorMonitoringTask::notifyAccessGranted() {
    accessGranted = true;
    unlockTime = millis();
    doorCloseTime = 0;
    alarmTriggered = false;
    relay.unlock();
    Serial.println("[DOOR_MONITOR] Access granted - door unlocked");
}

void DoorMonitoringTask::notifyAccessRevoked() {
    accessGranted = false;
    relay.lock();
    Serial.println("[DOOR_MONITOR] Access revoked - door locked");
}

bool DoorMonitoringTask::isAccessGranted() const {
    return accessGranted;
}

void DoorMonitoringTask::monitoringTaskFunction(void* param) {
    DoorMonitoringTask* instance = static_cast<DoorMonitoringTask*>(param);
    Serial.println("[DOOR_MONITOR] Monitoring task running...");
    instance->monitorLoop();
    vTaskDelete(NULL);
}

void DoorMonitoringTask::monitorLoop() {
    // Initial delay to let device register first
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Report initial state
    lastReportedLockState = relay.isUnlocked();
    lockStateChangeTime = millis();
    lastDoorOpen = door.isOpen();
    reportStatus();
    
    while (running) {
        unsigned long now = millis();
        bool currentLockState = relay.isUnlocked();
        
        // Check for lock state change
        if (currentLockState != lastReportedLockState) {
            Serial.print("[DOOR_MONITOR] 🔄 Lock state changed: ");
            Serial.println(currentLockState ? "UNLOCKED" : "LOCKED");
            
            lastReportedLockState = currentLockState;
            lockStateChangeTime = now;
            
            // Only defer LOCK reports during countdown
            // Always report UNLOCK immediately so backend knows door is open
            bool isCountingDown = (accessGranted && doorCloseTime > 0 && !door.isOpen());
            bool shouldDefer = (!currentLockState && isCountingDown);  // LOCK during countdown
            
            if (!shouldDefer) {
                reportStatus();
            } else {
                Serial.println("[DOOR_MONITOR] Deferring LOCK report until countdown completes");
            }
        }
        
        // Check alarm conditions
        checkAlarms();
        
        // Handle relock logic
        handleRelockLogic();
        
        // NOTE: Status reports are ONLY sent on lock state changes (see line ~112)
        // No periodic reports to avoid spam when API is offline
        
        // Check every 100ms
        vTaskDelay(pdMS_TO_TICKS(DOOR_MONITORING_CHECK_INTERVAL_MS));
    }
}

void DoorMonitoringTask::checkAlarms() {
    unsigned long now = millis();
    bool isDoorOpen = door.isOpen();
    bool isLocked = !relay.isUnlocked();
    
    // Track door state changes for logging only
    if (isDoorOpen != lastDoorOpen) {
        if (!isDoorOpen) {
            Serial.println("[DOOR_MONITOR] Door closed");
        } else {
            Serial.println("[DOOR_MONITOR] Door opened");
        }
        lastDoorOpen = isDoorOpen;
    }
    
    bool shouldAlarm = false;
    String alarmReason = "";
    
    // Case 1: Mismatch - Door OPEN but Lock CLOSED (immediate alarm)
    if (isDoorOpen && isLocked) {
        shouldAlarm = true;
        alarmReason = "MISMATCH - Door open but locked";
    }
    // Case 2: Timeout - Door OPEN for too long after unlock
    else if (accessGranted && isDoorOpen) {
        unsigned long timeSinceUnlock = now - unlockTime;
        if (timeSinceUnlock >= MAX_UNLOCK_DURATION_MS) {
            shouldAlarm = true;
            alarmReason = "TIMEOUT - Door open beyond limit (" + String(timeSinceUnlock/1000) + "s)";
        }
    }
    
    // Trigger or stop alarm
    if (shouldAlarm) {
        if (!alarmTriggered) {
            Serial.print("[DOOR_MONITOR] ⚠️ ALARM: ");
            Serial.println(alarmReason);
            buzzer.startAlarm();
            lcd.show("ALARM!", alarmReason.substring(0, 16));
            alarmTriggered = true;
        }
        // Don't update LCD if alarm is already active - let it show alarm message
    } else {
        if (alarmTriggered || buzzer.isAlarmActive()) {
            Serial.println("[DOOR_MONITOR] ✓ Stopping alarm - condition cleared");
            buzzer.stopAlarm();
            alarmTriggered = false;
            // LCD will be updated by handleRelockLogic in next iteration
        }
    }
}

void DoorMonitoringTask::handleRelockLogic() {
    unsigned long now = millis();
    bool isDoorOpen = door.isOpen();
    
    if (!accessGranted) {
        // No active access session, but check if we should show idle state
        // Skip LCD update if in config mode
        static unsigned long lastIdleUpdate = 0;
        if (!inConfigMode && !isDoorOpen && (now - lastIdleUpdate > 2000)) {
            // Door is closed and no access granted - show idle state
            lcd.show("Locked", "Tap a card");
            lastIdleUpdate = now;
        }
        return;
    }
    
    // Skip all LCD updates if in config mode
    if (inConfigMode) {
        return;
    }
    
    // Force relock on timeout
    if (now >= (unlockTime + MAX_UNLOCK_DURATION_MS)) {
        Serial.println("[DOOR_MONITOR] Force relock - timeout");
        relay.lock();
        accessGranted = false;
        lcd.show("Timeout", "Relocked");
        // Will show "Locked / Tap a card" in next iteration when door is closed
        return;
    }
    
    if (isDoorOpen) {
        // Door is open, keep unlocked and show status
        lcd.show("Door Open", "Please close");
        doorCloseTime = 0;  // Reset close timer
    } else {
        // Door is closed
        if (doorCloseTime == 0) {
            // Just closed, start countdown
            doorCloseTime = now;
            Serial.println("[DOOR_MONITOR] Door closed, relocking in 3s");
            lcd.show("Door closed", "Relock in 3s");
        } else {
            // Check if relock time reached
            unsigned long timeSinceClose = now - doorCloseTime;
            if (timeSinceClose >= RELOCK_DELAY_MS) {
                Serial.println("[DOOR_MONITOR] Auto relock");
                relay.lock();
                accessGranted = false;
                lcd.show("Locked", "Tap a card");
                
                // Report status now that countdown is complete
                reportStatus();
            } else {
                // Show countdown - only update when number changes
                static int lastDisplayedSec = -1;
                int remainSec = (RELOCK_DELAY_MS - timeSinceClose) / 1000;  // Truncate, not round up
                
                // Only update LCD when countdown value actually changes
                if (remainSec != lastDisplayedSec) {
                    String msg = "Relock in " + String(remainSec) + "s";
                    lcd.show("Door closed", msg);
                    lastDisplayedSec = remainSec;
                }
            }
        }
    }
}

bool DoorMonitoringTask::reportStatus() {
    // Skip reporting if API is offline to reduce spam
    if (api.isOffline()) {
        static unsigned long lastSkipLog = 0;
        unsigned long now = millis();
        if (now - lastSkipLog > 60000) {  // Log once per minute
            Serial.println("[DOOR_MONITOR] Skipping status report - API offline");
            lastSkipLog = now;
        }
        return false;
    }
    
    bool isOpen = relay.isUnlocked();
    bool isOnline = true;
    
    bool success = api.updateDoorStatus(doorId, isOpen, isOnline);
    
    if (success) {
        lastReportTime = millis();
    }
    
    return success;
}
