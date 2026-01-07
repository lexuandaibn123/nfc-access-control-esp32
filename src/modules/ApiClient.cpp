#include "ApiClient.h"
#include "config.h"
#include <WiFi.h>
#include <time.h>

ApiClient::ApiClient(const char* baseUrl)
    : baseUrl(baseUrl), consecutiveFailures(0) {
    secureClient.setInsecure();
}

void ApiClient::setDeviceToken(const String& token) {
    deviceToken = token;
}

void ApiClient::getDeviceToken(String& outToken) const {
    outToken = deviceToken;
}

void ApiClient::setBaseUrl(const char* newBaseUrl) {
    baseUrl = newBaseUrl;
    Serial.print("[ApiClient] Base URL updated to: ");
    Serial.println(baseUrl);
}

bool ApiClient::registerDevice(String& outToken) {
    JsonDocument requestDoc;
    requestDoc["device_id"] = DEVICE_ID;
    requestDoc["secret"] = DEVICE_SECRET;
    requestDoc["hardware_type"] = HARDWARE_TYPE;
    requestDoc["firmware_version"] = FIRMWARE_VERSION;
    requestDoc["door_id"] = DOOR_ID;
    
    JsonDocument responseDoc;
    if (!post("/device/register", requestDoc, responseDoc)) {
        return false;
    }
    
    // Extract device_token
    if (responseDoc.containsKey("data") && responseDoc["data"].containsKey("device_token")) {
        outToken = responseDoc["data"]["device_token"].as<String>();
        return true;
    }
    
    Serial.println("[API] Missing device_token in response");
    return false;
}

bool ApiClient::getConfig(DeviceConfig& outConfig) {
    JsonDocument responseDoc;
    if (!get("/device/config", responseDoc)) {
        return false;
    }
    
    // API returns: {"success": true, "data": {...}}
    JsonObject data = responseDoc["data"].as<JsonObject>();
    
    outConfig.device_id = data["device_id"].as<String>();
    outConfig.relay_open_ms = data["relay_open_ms"] | 3000;
    
    // Offline mode config
    if (data.containsKey("offline_mode")) {
        outConfig.offline_mode.enabled = data["offline_mode"]["enabled"] | false;
        outConfig.offline_mode.cache_expire_at = data["offline_mode"]["cache_expire_at"].as<String>();
    }
    
    // JWT config
    if (data.containsKey("jwt_verification")) {
        outConfig.jwt_verification.alg = data["jwt_verification"]["alg"].as<String>();
        outConfig.jwt_verification.public_key_pem = data["jwt_verification"]["public_key_pem"].as<String>();
        outConfig.jwt_verification.kid = data["jwt_verification"]["kid"].as<String>();
    }
    
    // Offline whitelist
    outConfig.whitelist_count = 0;
    if (data.containsKey("offline_whitelist")) {
        JsonArray whitelist = data["offline_whitelist"].as<JsonArray>();
        int count = 0;
        
        for (JsonObject item : whitelist) {
            if (count >= 50) break;  // Max 50 entries
            
            outConfig.whitelist[count].card_id = item["card_id"].as<String>();
            outConfig.whitelist[count].user_id = item["user_id"] | "";
            outConfig.whitelist[count].valid_until = item["valid_until"] | "";
            count++;
        }
        
        outConfig.whitelist_count = count;
        Serial.print("[API] Parsed whitelist: ");
        Serial.print(count);
        Serial.println(" entries");
    }
    
    return true;
}

bool ApiClient::sendHeartbeat(const DeviceStatus& status) {
    JsonDocument requestDoc;
    requestDoc["device_id"] = DEVICE_ID;
    requestDoc["timestamp"] = getTimestamp();
    requestDoc["status"]["uptime_sec"] = status.uptime_sec;
    requestDoc["status"]["rssi"] = status.rssi;
    requestDoc["status"]["fw_version"] = status.fw_version;
    
    // Omit empty last_access_ts
    if (status.last_access_ts.length() > 0) {
        requestDoc["status"]["last_access_ts"] = status.last_access_ts;
    }
    
    JsonDocument responseDoc;
    return post("/device/heartbeat", requestDoc, responseDoc);
}

bool ApiClient::checkAccess(const AccessCheckRequest& request, AccessCheckResponse& outResponse) {
    JsonDocument requestDoc;
    requestDoc["device_id"] = request.device_id;
    requestDoc["door_id"] = request.door_id;
    requestDoc["timestamp"] = request.timestamp;
    
    if (request.card_id.length() > 0) {
        requestDoc["card_id"] = request.card_id;
    }
    if (request.card_uid.length() > 0) {
        requestDoc["card_uid"] = request.card_uid;
    }
    if (request.credential_raw.length() > 0) {
        requestDoc["credential"]["raw"] = request.credential_raw;
        requestDoc["credential"]["format"] = "jwt";
    }
    
    JsonDocument responseDoc;
    if (!post("/access/check", requestDoc, responseDoc)) {
        return false;
    }
    
    // Backend returns: {"success": true, "data": {...}}
    JsonObject data = responseDoc.containsKey("data") ? responseDoc["data"].as<JsonObject>() : responseDoc.as<JsonObject>();
    
    outResponse.result = data["result"].as<String>();
    outResponse.reason = data["reason"].as<String>();
    outResponse.relay_open_ms = data["relay_open_ms"] | 3000;
    
    outResponse.has_user = data.containsKey("user");
    if (outResponse.has_user) {
        outResponse.user.user_id = data["user"]["user_id"].as<String>();
        outResponse.user.name = data["user"]["name"].as<String>();
    }
    
    outResponse.has_policy = data.containsKey("policy");
    if (outResponse.has_policy) {
        outResponse.policy.access_level = data["policy"]["access_level"].as<String>();
        outResponse.policy.valid_until = data["policy"]["valid_until"].as<String>();
    }
    
    outResponse.has_credential = data.containsKey("credential");
    if (outResponse.has_credential) {
        outResponse.credential.format = data["credential"]["format"].as<String>();
        outResponse.credential.alg = data["credential"]["alg"].as<String>();
        outResponse.credential.raw = data["credential"]["raw"].as<String>();
        outResponse.credential.exp = data["credential"]["exp"].as<String>();
    }
    
    return true;
}

bool ApiClient::createCard(const CardCreateRequest& request, CardCreateResponse& outResponse) {
    JsonDocument requestDoc;
    requestDoc["device_id"] = request.device_id;
    requestDoc["card_uid"] = request.card_uid;
    
    JsonDocument responseDoc;
    if (!post("/cards", requestDoc, responseDoc)) {
        return false;
    }
    
    JsonObject data;
    
    // Backend returns: {"success": true, "data": {"card_id": "...", ...}}
    if (responseDoc.containsKey("data")) {
        data = responseDoc["data"];
    }
    // Or on 409: {"success": false, "error": {"details": {"existing_card": {...}}}}
    else if (responseDoc.containsKey("error") && 
             responseDoc["error"].containsKey("details") && 
             responseDoc["error"]["details"].containsKey("existing_card")) {
        data = responseDoc["error"]["details"]["existing_card"];
        Serial.println("[API] Card already exists, using existing data");
    }
    else {
        Serial.println("[API] Missing data in card response");
        return false;
    }
    
    outResponse.card_id = data["card_id"].as<String>();
    outResponse.card_uid = data["card_uid"].as<String>();
    outResponse.user_id = data["user_id"] | "";
    outResponse.status = data["status"].as<String>();
    outResponse.enroll_mode = data["enroll_mode"] | false;
    
    return true;
}

bool ApiClient::uploadLogs(LogEntry* logs, int count) {
    JsonDocument requestDoc;
    requestDoc["device_id"] = DEVICE_ID;
    
    JsonArray logsArray = requestDoc["logs"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject logObj = logsArray.add<JsonObject>();
        logObj["ts"] = logs[i].ts;
        logObj["door_id"] = logs[i].door_id;
        logObj["card_id"] = logs[i].card_id;
        logObj["card_uid"] = logs[i].card_uid;
        logObj["decision"] = logs[i].decision;
        logObj["reason"] = logs[i].reason;
    }
    
    JsonDocument responseDoc;
    return post("/access/log-batch", requestDoc, responseDoc);
}

bool ApiClient::pollDoorCommand(const String& doorId, DoorCommandPollResponse& outResponse) {
    // Critical: Local clients generally required for thread safety here
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[POLL] WiFi not connected");
        return false;
    }
    
    // Build endpoint: /doors/{doorId}/command/poll
    String endpoint = "/doors/" + doorId + "/command/poll";
    String url = String(baseUrl) + endpoint;
    
    // Local SSL client
    WiFiClientSecure secureClientLocal;
    secureClientLocal.setInsecure();
    secureClientLocal.setTimeout(15);
    secureClientLocal.setHandshakeTimeout(15);
    
    // Create LOCAL HTTP client
    HTTPClient httpLocal;
    
    // Use local secure client for HTTPS URLs
    if (url.startsWith("https://")) {
        httpLocal.begin(secureClientLocal, url);
    } else {
        httpLocal.begin(url);
    }
    
    // Set long timeout (35s - larger than backend's 30s)
    httpLocal.setTimeout(COMMAND_POLL_TIMEOUT_MS);
    httpLocal.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpLocal.setRedirectLimit(3);
    
    if (deviceToken.length() > 0) {
        httpLocal.addHeader("Authorization", "Bearer " + deviceToken);
    }
    
    Serial.println("====================================");
    Serial.println("[POLL] ⏳ Long polling started... (waiting for command)");
    unsigned long startTime = millis();
    
    // This will BLOCK until command arrives or timeout (30s)
    int httpCode = httpLocal.GET();
    
    unsigned long elapsed = millis() - startTime;
    Serial.print("[POLL] 📨 Response received after ");
    Serial.print(elapsed);
    Serial.println("ms");
    
    if (httpCode > 0) {
        if (httpCode == 200) {
            String response = httpLocal.getString();
            
            Serial.println("[POLL] Response body:");
            Serial.println(response);
            Serial.println("====================================\n");
            
            JsonDocument responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            httpLocal.end();
            
            if (error) {
                Serial.print("[POLL] JSON parse error: ");
                Serial.println(error.c_str());
                return false;
            }
            
            // Parse response: {"success": true, "data": {"hasCommand": bool, ...}}
            JsonObject data = responseDoc["data"].as<JsonObject>();
            
            outResponse.hasCommand = data["hasCommand"] | false;
            outResponse.waitTime = data["waitTime"] | 0;
            
            if (outResponse.hasCommand) {
                // Parse command object
                JsonObject cmdObj = data["command"].as<JsonObject>();
                outResponse.command.action = cmdObj["action"].as<String>();
                outResponse.command.timestamp = cmdObj["timestamp"].as<String>();
                outResponse.command.requestedBy = cmdObj["requestedBy"] | "";
                
                Serial.print("[POLL] 🚪 Command received: ");
                Serial.print(outResponse.command.action);
                Serial.print(" from ");
                Serial.println(outResponse.command.requestedBy);
            } else {
                Serial.println("[POLL] ⏱️ No command (timeout)");
            }
            
            // Ignore success/failure for polling (timeouts expected)
            return true;
        } else {
            String errorBody = httpLocal.getString();
            Serial.print("[POLL] HTTP error ");
            Serial.println(httpCode);
            Serial.println(errorBody);
            Serial.println("====================================\n");
            httpLocal.end();
            return false;
        }
    } else {
        Serial.print("[POLL] Connection error: ");
        Serial.println(httpLocal.errorToString(httpCode));
        Serial.println("====================================\n");
        httpLocal.end();
        return false;
    }
}

bool ApiClient::acknowledgeDoorCommand(const String& doorId, bool success) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[POLL] WiFi not connected");
        return false;
    }
    
    // Build endpoint: /doors/{doorId}/command/ack
    String endpoint = "/doors/" + doorId + "/command/ack";
    String url = String(baseUrl) + endpoint;
    
    // Critical: Local clients to avoid races
    WiFiClientSecure secureClientLocal;
    HTTPClient httpLocal;
    
    // HTTP/HTTPS support
    if (url.startsWith("https://")) {
        secureClientLocal.setInsecure();
        secureClientLocal.setTimeout(15);
        secureClientLocal.setHandshakeTimeout(15);
        httpLocal.begin(secureClientLocal, url);
    } else {
        httpLocal.begin(url);  // Plain HTTP
    }
    
    httpLocal.setTimeout(API_TIMEOUT_MS);
    httpLocal.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpLocal.setRedirectLimit(3);
    
    if (deviceToken.length() > 0) {
        httpLocal.addHeader("Authorization", "Bearer " + deviceToken);
    }
    httpLocal.addHeader("Content-Type", "application/json");
    
    JsonDocument requestDoc;
    requestDoc["success"] = success;
    
    String payload;
    serializeJson(requestDoc, payload);
    
    int httpCode = httpLocal.POST(payload);
    
    bool result = (httpCode >= 200 && httpCode < 300);
    
    if (result) {
        Serial.println("[POLL] ✅ Command acknowledged");
    } else {
        Serial.print("[POLL] ❌ Failed to acknowledge command (HTTP ");
        Serial.print(httpCode);
        Serial.println(")");
    }
    
    httpLocal.end();
    
    return result;
}


bool ApiClient::isOffline() const {
    return consecutiveFailures >= MAX_API_FAILURES;
}

int ApiClient::getFailureCount() const {
    return consecutiveFailures;
}

bool ApiClient::checkHealth() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    // Skip if already offline to reduce spam
    if (isOffline()) {
        static unsigned long lastOfflineLog = 0;
        unsigned long now = millis();
        if (now - lastOfflineLog > 60000) {  // Log once per minute
            Serial.println("[HEALTH] Already offline, skipping health check");
            lastOfflineLog = now;
        }
        return false;
    }
    
    HTTPClient http;
    http.setTimeout(2000);  // Fast timeout (2s)
    
    String url = String(baseUrl) + "/health";
    
    // Support both HTTP and HTTPS
    if (url.startsWith("https://")) {
        http.begin(secureClient, url);
    } else {
        http.begin(url);  // Plain HTTP
    }
    
    int httpCode = http.GET();
    http.end();
    if (url.startsWith("https://")) {
        secureClient.stop();
    }
    
    bool healthy = (httpCode == 200 || httpCode == 404);  // 404 means backend reachable but no /health endpoint
    
    if (healthy) {
        Serial.println("[HEALTH] API is reachable");
        recordSuccess();  // Reset failure count
    } else {
        // Log first failure, then every 10th
        if (consecutiveFailures == 0 || consecutiveFailures % 10 == 0) {
            Serial.println("[HEALTH] API unreachable");
        }
        recordFailure();
    }
    
    return healthy;
}

// ============================================
// Door Status Reporting
// ============================================
bool ApiClient::updateDoorStatus(const String& doorId, bool isOpen, bool isOnline) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    // Skip if already offline to reduce spam
    if (isOffline()) {
        return false;
    }
    
    String endpoint = "/doors/" + doorId + "/status";
    String url = String(baseUrl) + endpoint;
    
    // WiFiClientSecure MUST be declared outside if/else to prevent premature destruction
    WiFiClientSecure secureClientLocal;
    HTTPClient httpLocal;
    
    // Support both HTTP and HTTPS
    if (url.startsWith("https://")) {
        secureClientLocal.setInsecure();
        secureClientLocal.setTimeout(15);
        secureClientLocal.setHandshakeTimeout(15);
        httpLocal.begin(secureClientLocal, url);
    } else {
        httpLocal.begin(url);  // Plain HTTP
    }
    
    httpLocal.setTimeout(API_TIMEOUT_MS);
    httpLocal.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpLocal.setRedirectLimit(3);
    
    if (deviceToken.length() > 0) {
        httpLocal.addHeader("Authorization", "Bearer " + deviceToken);
    }
    httpLocal.addHeader("Content-Type", "application/json");
    
    JsonDocument requestDoc;
    requestDoc["isOpen"] = isOpen;
    requestDoc["isOnline"] = isOnline;
    
    String payload;
    serializeJson(requestDoc, payload);
    
    int httpCode = httpLocal.PUT(payload);
    httpLocal.end();
    
    if (httpCode >= 200 && httpCode < 300) {
        recordSuccess();
        return true;
    } else {
        recordFailure();
        return false;
    }
}

bool ApiClient::post(const char* endpoint, const JsonDocument& requestDoc, JsonDocument& responseDoc) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] WiFi not connected");
        recordFailure();
        return false;
    }
    
    String url = String(baseUrl) + endpoint;
    
    // Use secure client for HTTPS URLs
    if (url.startsWith("https://")) {
        http.begin(secureClient, url);
    } else {
        http.begin(url);
    }
    
    // Follow redirects (ngrok redirects HTTP to HTTPS)
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(3);
    http.setTimeout(API_TIMEOUT_MS);  // Use configured timeout
    
    http.addHeader("Content-Type", "application/json");
    
    if (deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + deviceToken);
    }
    
    String requestBody;
    serializeJson(requestDoc, requestBody);
    
    Serial.println("====================================");
    Serial.print("[API_REQ] POST ");
    Serial.println(endpoint);
    Serial.print("[API_REQ] URL: ");
    Serial.println(url);
    if (deviceToken.length() > 0) {
        Serial.print("[API_REQ] Auth: Bearer ");
        Serial.println(deviceToken.substring(0, 20) + "...");
    }
    Serial.println("[API_REQ] Body:");
    serializeJsonPretty(requestDoc, Serial);
    Serial.println();
    Serial.println("------------------------------------");
    
    int httpCode = http.POST(requestBody);
    
    if (httpCode > 0) {
        // Parse response for 200, 201, and 409 (conflict - existing resource)
        if (httpCode == 200 || httpCode == 201 || httpCode == 409) {
            String response = http.getString();
            
            Serial.print("[API_RES] HTTP ");
            Serial.println(httpCode);
            Serial.println("[API_RES] Body:");
            Serial.println(response);
            Serial.println("====================================\n");
            
            DeserializationError error = deserializeJson(responseDoc, response);
            http.end();
            
            if (error) {
                Serial.print("[API_RES] JSON parse error: ");
                Serial.println(error.c_str());
                recordFailure();
                return false;
            }
            
            // For 409, still return true so caller can extract data from response
            if (httpCode == 409) {
                Serial.println("[API_RES] Note: Resource already exists (409 Conflict)");
            }
            
            recordSuccess();
            return true;
        } else {
            String errorBody = http.getString();
            Serial.print("[API_RES] HTTP error ");
            Serial.println(httpCode);
            Serial.println("[API_RES] Error body:");
            Serial.println(errorBody);
            Serial.println("====================================\n");
            http.end();
            secureClient.stop();
            recordFailure();
            return false;
        }
    } else {
        Serial.print("[API_RES] Connection error: ");
        Serial.println(http.errorToString(httpCode));
        Serial.println("====================================\n");
        http.end();
        secureClient.stop();
        recordFailure();
        return false;
    }
}

bool ApiClient::get(const char* endpoint, JsonDocument& responseDoc) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] WiFi not connected");
        recordFailure();
        return false;
    }
    
    String url = String(baseUrl) + endpoint;
    
    // Use secure client for HTTPS URLs
    if (url.startsWith("https://")) {
        http.begin(secureClient, url);
    } else {
        http.begin(url);
    }
    
    // Follow redirects (ngrok redirects HTTP to HTTPS)
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(3);
    http.setTimeout(API_TIMEOUT_MS);  // Use configured timeout
    
    if (deviceToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + deviceToken);
    }
    
    Serial.println("====================================");
    Serial.print("[API_REQ] GET ");
    Serial.println(endpoint);
    Serial.print("[API_REQ] URL: ");
    Serial.println(url);
    if (deviceToken.length() > 0) {
        Serial.print("[API_REQ] Auth: Bearer ");
        Serial.println(deviceToken.substring(0, 20) + "...");
    }
    Serial.println("------------------------------------");
    
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == 200) {
            String response = http.getString();
            
            Serial.print("[API_RES] HTTP ");
            Serial.println(httpCode);
            Serial.println("[API_RES] Body:");
            Serial.println(response);
            Serial.println("====================================\n");
            
            DeserializationError error = deserializeJson(responseDoc, response);
            http.end();
            secureClient.stop();
            
            if (error) {
                Serial.print("[API_RES] JSON parse error: ");
                Serial.println(error.c_str());
                recordFailure();
                return false;
            }
            
            recordSuccess();
            return true;
        } else {
            String errorBody = http.getString();
            Serial.print("[API_RES] HTTP error ");
            Serial.println(httpCode);
            Serial.println("[API_RES] Error body:");
            Serial.println(errorBody);
            Serial.println("====================================\n");
            http.end();
            secureClient.stop();
            recordFailure();
            return false;
        }
    } else {
        Serial.print("[API_RES] Connection error: ");
        Serial.println(http.errorToString(httpCode));
        Serial.println("====================================\n");
        http.end();
        secureClient.stop();
        recordFailure();
        return false;
    }
}

void ApiClient::recordFailure() {
    consecutiveFailures++;
    
    // Rate limit failure logs to reduce spam
    static unsigned long lastFailureLog = 0;
    unsigned long now = millis();
    
    // Log first 3 failures, then every 10th, then once per minute after offline
    bool shouldLog = (consecutiveFailures <= 3) || 
                     (consecutiveFailures < MAX_API_FAILURES && consecutiveFailures % 5 == 0) ||
                     (consecutiveFailures >= MAX_API_FAILURES && (now - lastFailureLog > 60000));
    
    if (shouldLog) {
        Serial.print("[API] Consecutive failures: ");
        Serial.println(consecutiveFailures);
        lastFailureLog = now;
    }
}

void ApiClient::recordSuccess() {
    if (consecutiveFailures > 0) {
        Serial.println("[API] Connection restored");
        consecutiveFailures = 0;
    }
}

String ApiClient::getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "2025-01-01T00:00:00Z";  // Fallback
    }
    
    char buffer[25];
    memset(buffer, 0, sizeof(buffer));  // Clear buffer to avoid garbage
    strftime(buffer, sizeof(buffer) - 1, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    buffer[24] = '\0';  // Ensure null terminator
    return String(buffer);
}
