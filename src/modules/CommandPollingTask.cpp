#include "CommandPollingTask.h"

// Static mutex initialization
SemaphoreHandle_t CommandPollingTask::accessMutex = NULL;

CommandPollingTask::CommandPollingTask(ApiClient& api, AccessController& access, 
                                       LCDDisplay& lcd, RelayControl& relay)
    : api(api), access(access), lcd(lcd), relay(relay), taskHandle(NULL), running(false) {
    
    // Create mutex if not already created
    if (accessMutex == NULL) {
        accessMutex = xSemaphoreCreateMutex();
        if (accessMutex == NULL) {
            Serial.println("[POLL_TASK] ERROR: Failed to create mutex!");
        } else {
            Serial.println("[POLL_TASK] Mutex created successfully");
        }
    }
}

void CommandPollingTask::begin() {
    if (taskHandle != NULL) {
        Serial.println("[POLL_TASK] Task already running");
        return;
    }
    
    running = true;
    
    // Create FreeRTOS task
    // Stack: 8KB, Priority: 1 (same as loop), Core: 1 (same as loop)
    BaseType_t result = xTaskCreatePinnedToCore(
        pollingTaskFunction,           // Task function
        "CommandPoll",                 // Task name
        COMMAND_POLL_TASK_STACK_SIZE,  // Stack size (8192 bytes)
        this,                          // Task parameter (this pointer)
        COMMAND_POLL_TASK_PRIORITY,    // Priority
        &taskHandle,                   // Task handle
        1                              // Core 1 (same as loop)
    );
    
    if (result == pdPASS) {
        Serial.println("[POLL_TASK] ✅ Task created and started on core 1");
    } else {
        Serial.println("[POLL_TASK] ❌ Failed to create task!");
        running = false;
    }
}

void CommandPollingTask::stop() {
    if (taskHandle == NULL) {
        return;
    }
    
    running = false;
    
    // Wait a bit for task to finish current operation
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Delete task
    vTaskDelete(taskHandle);
    taskHandle = NULL;
    
    Serial.println("[POLL_TASK] Task stopped");
}

// Static task function - FreeRTOS entry point
void CommandPollingTask::pollingTaskFunction(void* param) {
    CommandPollingTask* instance = static_cast<CommandPollingTask*>(param);
    
    Serial.println("[POLL_TASK] Polling task running...");
    
    // Run poll loop
    instance->pollLoop();
    
    // Should never reach here
    Serial.println("[POLL_TASK] Task exiting (unexpected!)");
    vTaskDelete(NULL);
}

void CommandPollingTask::pollLoop() {
    // Small initial delay to let main system stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (running) {
        // Check if we have device token and API is online
        String currentToken;
        api.getDeviceToken(currentToken);
        
        if (currentToken.length() == 0) {
            // No token yet, wait and retry
            Serial.println("[POLL_TASK] ⏸️ No device token, waiting...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        if (api.isOffline()) {
            // API is offline, wait for recovery
            Serial.println("[POLL_TASK] ⏸️ API offline, waiting...");
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        
        // Poll for door command (this will block for up to 30s - that's OK in separate task!)
        DoorCommandPollResponse response;
        if (api.pollDoorCommand(DOOR_ID, response)) {
            if (response.hasCommand) {
                // Execute the command
                if (response.command.action == "unlock") {
                    Serial.println("[POLL_TASK] 🔓 Executing unlock command");
                    
                    // CRITICAL: Acquire mutex before modifying shared state
                    if (xSemaphoreTake(accessMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                        // Grant access - this resets any existing countdown
                        access.grantAccess("REMOTE_COMMAND");
                        
                        // Release mutex - main loop will handle LCD updates from now on
                        xSemaphoreGive(accessMutex);
                        
                        // Acknowledge command execution
                        api.acknowledgeDoorCommand(DOOR_ID, true);
                        
                        // Note: No need to restore LCD - AccessController.update() will handle it
                    } else {
                        Serial.println("[POLL_TASK] ⚠️ Failed to acquire mutex (timeout)");
                        api.acknowledgeDoorCommand(DOOR_ID, false);
                    }
                    
                } else if (response.command.action == "lock") {
                    Serial.println("[POLL_TASK] 🔒 Executing lock command");
                    
                    if (xSemaphoreTake(accessMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                        // Force lock
                        relay.lock();
                        
                        // Release mutex - main loop will handle LCD updates and alarm logic
                        xSemaphoreGive(accessMutex);
                        
                        // Acknowledge command execution
                        api.acknowledgeDoorCommand(DOOR_ID, true);
                        
                        // Note: No need to restore LCD - AccessController.update() will handle it
                    } else {
                        Serial.println("[POLL_TASK] ⚠️ Failed to acquire mutex (timeout)");
                        api.acknowledgeDoorCommand(DOOR_ID, false);
                    }
                    
                } else {
                    Serial.print("[POLL_TASK] ⚠️ Unknown command: ");
                    Serial.println(response.command.action);
                    api.acknowledgeDoorCommand(DOOR_ID, false);
                }
            }
            // else: timeout (no command) - normal, just continue polling
        } else {
            // Poll failed (network error, etc.)
            Serial.println("[POLL_TASK] ❌ Poll failed, retrying...");
            vTaskDelay(pdMS_TO_TICKS(5000));  // Wait before retry
        }
        
        // Small delay between polls
        vTaskDelay(pdMS_TO_TICKS(COMMAND_POLL_RETRY_DELAY_MS));
    }
}
