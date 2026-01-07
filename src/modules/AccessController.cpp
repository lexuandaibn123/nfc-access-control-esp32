#include "AccessController.h"
#include "JWTVerifier.h"
#include "DoorMonitoringTask.h"

AccessController::AccessController(NFCReader& nfc, ApiClient& api, RelayControl& relay,
                                   LCDDisplay& lcd, BuzzerControl& buzzer, DoorSensor& door,
                                   DoorMonitoringTask& doorMonitor)
    : nfc(nfc), api(api), relay(relay), lcd(lcd), buzzer(buzzer), door(door), doorMonitor(doorMonitor),
      whitelistCount(0), logQueueCount(0) {
}

void AccessController::update() {
}

void AccessController::handleCardTap() {
    uint32_t tapStartMs = millis();
    
    CardData card = nfc.readCard();
    uint32_t readMs = millis() - tapStartMs;

    Serial.print("[PERF] Card UID: ");
    Serial.print(card.card_uid);
    Serial.print(" | NFC read: ");
    Serial.print(readMs);
    Serial.println("ms");

    lcd.show("Card detected", card.card_uid.substring(0, 15));

    // Blank card -> Enroll
    if (!card.has_card_id) {
        handleBlankCard(card.card_uid);
        return;
    }
    
    handleCardWithId(card);
}

void AccessController::handleBlankCard(const String& card_uid) {
    Serial.println("[NFC] Blank card detected, enrolling...");
    lcd.show("Blank card", "Keep on reader!");
    
    CardCreateRequest request;
    request.device_id = DEVICE_ID;
    request.card_uid = card_uid;
    
    CardCreateResponse response;
    bool success = false;
    
    if (api.createCard(request, response)) {
        Serial.print("[API] Card enrolled, ID: ");
        Serial.println(response.card_id);
        
        // Write card_id
        if (nfc.writeCardId(response.card_id)) {
            // Clear old credential (requires reconnect)
            if (nfc.reconnect()) {
                if (nfc.clearCredential()) {
                   Serial.println("[ENROLL] Old credential wiped");
                }
            } else {
                Serial.println("[ENROLL] Failed to reconnect for credential wipe (minor issue)");
            }
            
            lcd.show("Enrolled!", response.card_id);
            buzzer.accessGranted();
            delay(2000);
            lcd.show("Ready", "Tap again");
            success = true;
        } else {
            lcd.show("Write failed", "Card works anyway");
            buzzer.accessGranted();  // Still success, backend has it
            Serial.println("[NFC] Failed to write card_id (backend enrolled)");
            delay(2000);
            lcd.show("Ready", "Tap again");
            success = true;
        }
        
        // Enrollment log handled by backend
    } else {
        lcd.show("Enroll failed", "Try again");
        buzzer.accessDenied();
        Serial.println("[API] Failed to enroll card");
        delay(1500);
        lcd.show("Ready", "Tap a card");
    }
    
    nfc.haltCard();
    Serial.println(success ? "[ENROLL] Success - reader ready" : "[ENROLL] Failed - reader ready");
}

void AccessController::handleCardWithId(const CardData& card) {
    uint32_t stepStartMs = millis();
    
    AccessCheckRequest request;
    request.device_id = DEVICE_ID;
    request.door_id = DOOR_ID;
    request.card_id = card.card_id;
    request.card_uid = card.card_uid;
    request.credential_raw = card.has_credential ? card.credential.raw : "";
    request.timestamp = api.getTimestamp();
    
    AccessCheckResponse response;
    
    uint32_t apiStartMs = millis();
    bool apiSuccess = api.checkAccess(request, response);
    uint32_t apiDurationMs = millis() - apiStartMs;
    
    Serial.print("[PERF] API call: ");
    Serial.print(apiDurationMs);
    Serial.println("ms");
    
    if (!apiSuccess && !api.isOffline()) {
        // Network error, not yet offline
        lcd.show("Server error", "Try again");
        buzzer.accessDenied();
        queueLog(createLog("DENY", "SERVER_ERROR", card.card_id, card.card_uid));
        // CRITICAL: Halt card before returning
        nfc.haltCard();
        
        uint32_t totalMs = millis() - stepStartMs;
        Serial.print("[PERF] Total (error): ");
        Serial.print(totalMs);
        Serial.println("ms");
        return;
    }
    
    if (!apiSuccess && api.isOffline()) {
        // Offline mode - verify JWT and check whitelist
        Serial.println("[OFFLINE] API unavailable - verifying JWT");
        
        if (checkOfflineWhitelist(card.card_id, card)) {
            // Card is authorized via JWT verification
            Serial.print("[OFFLINE] Access granted for: ");
            Serial.println(card.card_id);
            
            lcd.show("OFFLINE MODE", "Access granted");
            grantAccess("OFFLINE_WHITELIST");
            queueLog(createLog("ALLOW", "OFFLINE_WHITELIST", card.card_id, card.card_uid));
        } else {
            // JWT verification failed or not in whitelist
            Serial.println("[OFFLINE] Access denied");
            lcd.show("OFFLINE", "Access denied");
            buzzer.accessDenied();
            queueLog(createLog("DENY", "OFFLINE_NOT_WHITELISTED", card.card_id, card.card_uid));
        }
        
        // CRITICAL: Halt card before returning
        nfc.haltCard();
        
        uint32_t totalMs = millis() - stepStartMs;
        Serial.print("[PERF] Total (offline): ");
        Serial.print(totalMs);
        Serial.println("ms");
        return;
    }
    
    // Update credential (key rotation, enrollment, etc.)
    if (response.has_credential) {
        bool needsUpdate = false;
        
        // Check if credential is different
        if (!card.has_credential) {
            needsUpdate = true;  // No credential on card
            Serial.println("[PERF] No credential on card, updating");
        } else if (card.credential.raw != response.credential.raw) {
            needsUpdate = true;  // Credential changed
            Serial.println("[PERF] Credential changed, updating");
        } else {
            Serial.println("[PERF] Credential unchanged, skipping write");
        }
        
        if (needsUpdate) {
            lcd.show("Updating card", "Keep on reader!");
            delay(50);  // Reduced from 100ms
            
            uint32_t writeStartMs = millis();
            // Try to write credential
            if (nfc.writeCredential(response.credential)) {
                uint32_t writeDurationMs = millis() - writeStartMs;
                Serial.print("[PERF] Credential write: ");
                Serial.print(writeDurationMs);
                Serial.println("ms");
                
                lcd.show("Card updated!", "Welcome");
                delay(150);  // Reduced from 800ms
            } else {
                Serial.println("[NFC] Failed to write credential");
                lcd.show("Update failed", "Access granted"); // Message might be "Update failed" but access logic follows
                delay(150);  // Reduced from 800ms
            }
        }
    }

    // Check access result
    if (response.result == "ALLOW") {
        Serial.println("[ACCESS] ALLOWED");
        
        if (response.has_user) {
            Serial.print("[ACCESS] User: ");
            Serial.println(response.user.name);
            lcd.show("Welcome", response.user.name);
        } else {
            lcd.show("Access granted", "Welcome");
        }
        
        grantAccess(response.reason);
        queueLog(createLog("ALLOW", response.reason, card.card_id, card.card_uid));
        
    } else {
        Serial.print("[ACCESS] DENIED - ");
        Serial.println(response.reason);
        
        // Card deleted: Clear ID to allow re-enrollment
        if (response.reason == "CARD_NOT_FOUND" || response.reason == "CARD_DELETED") {
            
            Serial.println("[RECOVERY] Card not found, clearing...");
            lcd.show("Card deleted", "Clearing...");
            
            if (nfc.reconnect()) {
                if (nfc.clearCardId()) {
                    lcd.show("Card cleared", "Tap to re-enroll");
                    buzzer.accessDenied();
                    delay(2000);
                    queueLog(createLog("DENY", "CARD_CLEARED_FOR_REENROLL", card.card_id, card.card_uid));
                } else {
                    lcd.show("Clear failed", "Contact admin");
                    buzzer.accessDenied();
                    delay(2000);
                    queueLog(createLog("DENY", response.reason, card.card_id, card.card_uid));
                }
            } else {
                Serial.println("[RECOVERY] Failed to reconnect to card");
                lcd.show("Clear failed", "Card removed?");
                buzzer.accessDenied();
                delay(2000);
            }
        } else {
            // Normal deny with reason
            lcd.show("Access denied", response.reason);
            buzzer.accessDenied();
            queueLog(createLog("DENY", response.reason, card.card_id, card.card_uid));
        }
    }
    
    // CRITICAL: Always halt card to prevent blocking reader
    nfc.haltCard();
    
    uint32_t totalMs = millis() - stepStartMs;
    Serial.print("[PERF] Total access check: ");
    Serial.print(totalMs);
    Serial.println("ms");
}

void AccessController::grantAccess(const String& reason) {
    buzzer.accessGranted();
    doorMonitor.notifyAccessGranted();
    Serial.print("[ACCESS] Access granted: ");
    Serial.println(reason);
}



void AccessController::queueLog(const LogEntry& log) {
    if (logQueueCount >= LOG_QUEUE_SIZE) {
        Serial.println("[LOG] Queue full, dropping oldest");
        // Shift array left
        for (int i = 1; i < LOG_QUEUE_SIZE; i++) {
            logQueue[i - 1] = logQueue[i];
        }
        logQueueCount--;
    }
    
    logQueue[logQueueCount++] = log;
}

int AccessController::getQueuedLogCount() const {
    return logQueueCount;
}

bool AccessController::uploadQueuedLogs() {
    if (logQueueCount == 0) return true;
    
    Serial.print("[LOG] Uploading ");
    Serial.print(logQueueCount);
    Serial.println(" logs");
    
    if (api.uploadLogs(logQueue, logQueueCount)) {
        Serial.println("[LOG] Upload successful");
        logQueueCount = 0;
        return true;
    }
    
    Serial.println("[LOG] Upload failed");
    return false;
}

LogEntry AccessController::createLog(const String& decision, const String& reason,
                                      const String& card_id, const String& card_uid) {
    LogEntry log;
    log.ts = api.getTimestamp();
    log.door_id = DOOR_ID;
    log.card_id = card_id;
    log.card_uid = card_uid;
    log.decision = decision;
    log.reason = reason;
    return log;
}

void AccessController::updateConfig(const DeviceConfig& config) {
    // Update offline whitelist
    whitelistCount = config.whitelist_count;
    if (whitelistCount > 50) whitelistCount = 50;  // Safety limit
    
    for (int i = 0; i < whitelistCount; i++) {
        whitelist[i] = config.whitelist[i];
    }
    
    // Store JWT public key for offline verification
    jwtPublicKeyPem = config.jwt_verification.public_key_pem;
    
    Serial.print("[CONFIG] Whitelist updated: ");
    Serial.print(whitelistCount);
    Serial.println(" entries");
    Serial.println("[CONFIG] JWT public key stored for offline verification");
}

bool AccessController::checkOfflineWhitelist(const String& card_id, const CardData& card) {
    // Offline: Verify JWT and card binding
    if (!card.has_credential) {
        Serial.println("[OFFLINE] No JWT - denied");
        return false;
    }
    
    Serial.println("[OFFLINE] Verifying JWT");
    JWTVerifier verifier;
    JWTPayload payload;
    
    if (!verifier.verify(card.credential.raw, jwtPublicKeyPem, payload)) {
        Serial.println("[OFFLINE] JWT verification failed");
        return false;
    }
    
    // Verify JWT matches card
    if (payload.card_uid != card.card_uid) {
        Serial.print("[OFFLINE] JWT card_uid mismatch - JWT: ");
        Serial.print(payload.card_uid);
        Serial.print(", Card: ");
        Serial.println(card.card_uid);
        Serial.println("[OFFLINE] Possible replay attack detected!");
        return false;
    }
    
    if (payload.card_id != card.card_id) {
        Serial.print("[OFFLINE] JWT card_id mismatch - JWT: ");
        Serial.print(payload.card_id);
        Serial.print(", Card: ");
        Serial.println(card.card_id);
        Serial.println("[OFFLINE] Possible replay attack detected!");
        return false;
    }
    
    Serial.println("[OFFLINE] JWT bound to card verified");
    
    // Check expiration
    unsigned long currentTime = millis() / 1000;
    if (payload.exp > 0 && currentTime > payload.exp) {
        Serial.println("[OFFLINE] JWT expired");
        return false;
    }
    
    // Check offline_max_until
    if (payload.offline_max_until > 0 && currentTime > payload.offline_max_until) {
        Serial.println("[OFFLINE] JWT offline period expired");
        return false;
    }
    
    // JWT is valid - check extracted card_id against whitelist
    Serial.print("[OFFLINE] JWT valid, card_id: ");
    Serial.println(payload.card_id);
    
    for (int i = 0; i < whitelistCount; i++) {
        if (whitelist[i].card_id == payload.card_id) {
            Serial.print("[OFFLINE] Card authorized, user: ");
            Serial.println(payload.user_id);
            return true;
        }
    }
    
    Serial.println("[OFFLINE] Card not in whitelist");
    return false;
}

