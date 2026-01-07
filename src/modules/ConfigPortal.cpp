#include "ConfigPortal.h"
#include "config.h"
#include <WiFi.h>

// AP Configuration
#define AP_CHANNEL 1
#define DNS_PORT 53
#define WEB_PORT 80

ConfigPortal::ConfigPortal(ConfigManager& configManager)
    : configManager(configManager), server(WEB_PORT), active(false) {
}

void ConfigPortal::generateAPSSID() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[7];
    sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    apSSID = "ESP32-NFC-" + String(macStr);
}

void ConfigPortal::begin() {
    if (active) return;
    
    generateAPSSID();
    
    Serial.println("[ConfigPortal] Starting Access Point...");
    Serial.print("[ConfigPortal] SSID: ");
    Serial.println(apSSID);
    Serial.print("[ConfigPortal] Password: ");
    Serial.println(CONFIG_AP_PASSWORD);
    
    // Start AP mode
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID.c_str(), CONFIG_AP_PASSWORD, AP_CHANNEL);
    
    delay(100);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("[ConfigPortal] AP IP address: ");
    Serial.println(IP);
    
    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", IP);
    
    // Setup web server routes
    setupRoutes();
    
    // Start web server
    server.begin();
    
    active = true;
    Serial.println("[ConfigPortal] Configuration portal started");
}

void ConfigPortal::stop() {
    if (!active) return;
    
    Serial.println("[ConfigPortal] Stopping configuration portal...");
    
    server.end();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    
    active = false;
    Serial.println("[ConfigPortal] Configuration portal stopped");
}

bool ConfigPortal::isActive() {
    return active;
}

void ConfigPortal::update() {
    if (active) {
        dnsServer.processNextRequest();
    }
}

String ConfigPortal::getAPSSID() {
    return apSSID;
}

void ConfigPortal::setupRoutes() {
    // Main pages
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleRoot(request);
    });
    
    server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleWiFiPage(request);
    });
    
    server.on("/device", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleDevicePage(request);
    });
    
    server.on("/api", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleAPIPage(request);
    });
    
    server.on("/system", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleSystemPage(request);
    });
    
    // API endpoints
    server.on("/status.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleStatus(request);
    });
    
    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSaveConfig(request);
    });
    
    server.on("/factory-reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleFactoryReset(request);
    });
    
    server.on("/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleRestart(request);
    });
    
    // Captive portal - redirect all other requests to root
    server.onNotFound([this](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
}

// ============================================
// Route Handlers
// ============================================

void ConfigPortal::handleRoot(AsyncWebServerRequest *request) {
    DeviceConfiguration config;
    configManager.load(config);
    
    String html = generateHeader("Dashboard", "home");
    
    html += "<div class='card'>";
    html += "<h2>ESP32 NFC Access Control</h2>";
    html += "<p class='subtitle'>Configuration Portal</p>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>Device Status</h3>";
    html += "<table>";
    html += "<tr><td>Configuration:</td><td>" + String(config.configured ? "✓ Configured" : "⚠ Not Configured") + "</td></tr>";
    html += "<tr><td>Device ID:</td><td>" + String(config.device_id) + "</td></tr>";
    html += "<tr><td>Door ID:</td><td>" + String(config.door_id) + "</td></tr>";
    html += "<tr><td>Hardware:</td><td>" + String(config.hardware_type) + "</td></tr>";
    html += "<tr><td>Firmware:</td><td>" + String(FIRMWARE_VERSION) + "</td></tr>";
    html += "</table>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>Network Status</h3>";
    html += "<table>";
    html += "<tr><td>WiFi SSID:</td><td>" + String(config.wifi_ssid) + "</td></tr>";
    html += "<tr><td>WiFi Status:</td><td>" + String(WiFi.status() == WL_CONNECTED ? "✓ Connected" : "✗ Disconnected") + "</td></tr>";
    if (WiFi.status() == WL_CONNECTED) {
        html += "<tr><td>IP Address:</td><td>" + WiFi.localIP().toString() + "</td></tr>";
        html += "<tr><td>Signal:</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
    }
    html += "</table>";
    html += "</div>";
    
    html += "<div class='card'>";
    html += "<h3>Quick Actions</h3>";
    html += "<a href='/wifi' class='btn'>Configure WiFi</a> ";
    html += "<a href='/device' class='btn'>Device Settings</a> ";
    html += "<a href='/system' class='btn btn-danger'>System</a>";
    html += "</div>";
    
    html += generateFooter();
    
    request->send(200, "text/html; charset=UTF-8", html);
}

void ConfigPortal::handleWiFiPage(AsyncWebServerRequest *request) {
    DeviceConfiguration config;
    configManager.load(config);
    
    String html = generateHeader("WiFi Configuration", "wifi");
    
    html += "<div class='card'>";
    html += "<h2>WiFi Configuration</h2>";
    html += "<form method='POST' action='/save'>";
    html += "<input type='hidden' name='page' value='wifi'>";
    
    html += "<div class='form-group'>";
    html += "<label>WiFi SSID:</label>";
    html += "<input type='text' name='wifi_ssid' value='" + String(config.wifi_ssid) + "' required maxlength='31'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label>WiFi Password:</label>";
    html += "<input type='password' name='wifi_password' value='" + String(config.wifi_password) + "' maxlength='63'>";
    html += "<small>Leave empty for open network</small>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-primary'>Save WiFi Settings</button>";
    html += "</form>";
    html += "</div>";
    
    html += generateFooter();
    
    request->send(200, "text/html; charset=UTF-8", html);
}

void ConfigPortal::handleDevicePage(AsyncWebServerRequest *request) {
    DeviceConfiguration config;
    configManager.load(config);
    
    String html = generateHeader("Device Configuration", "device");
    
    html += "<div class='card'>";
    html += "<h2>Device Configuration</h2>";
    html += "<form method='POST' action='/save'>";
    html += "<input type='hidden' name='page' value='device'>";
    
    html += "<div class='form-group'>";
    html += "<label>Device ID:</label>";
    html += "<input type='text' name='device_id' value='" + String(config.device_id) + "' required maxlength='31'>";
    html += "<small>Unique identifier for this device</small>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label>Device Secret:</label>";
    html += "<input type='password' name='device_secret' value='" + String(config.device_secret) + "' required maxlength='63'>";
    html += "<small>Authentication secret for API</small>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label>Door ID:</label>";
    html += "<input type='text' name='door_id' value='" + String(config.door_id) + "' required maxlength='31'>";
    html += "<small>Identifier for the door controlled by this device</small>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label>Hardware Type:</label>";
    html += "<input type='text' name='hardware_type' value='" + String(config.hardware_type) + "' maxlength='31'>";
    html += "<small>e.g., esp32-rc522</small>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-primary'>Save Device Settings</button>";
    html += "</form>";
    html += "</div>";
    
    html += generateFooter();
    
    request->send(200, "text/html; charset=UTF-8", html);
}

void ConfigPortal::handleAPIPage(AsyncWebServerRequest *request) {
    DeviceConfiguration config;
    configManager.load(config);
    
    String html = generateHeader("API Configuration", "api");
    
    html += "<div class='card'>";
    html += "<h2>API Configuration</h2>";
    html += "<form method='POST' action='/save'>";
    html += "<input type='hidden' name='page' value='api'>";
    
    html += "<div class='form-group'>";
    html += "<label>API Base URL:</label>";
    html += "<input type='url' name='api_base_url' value='" + String(config.api_base_url) + "' required maxlength='127'>";
    html += "<small>Backend API endpoint (e.g., https://your-domain.com/api/v1)</small>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-primary'>Save API Settings</button>";
    html += "</form>";
    html += "</div>";
    
    html += generateFooter();
    
    request->send(200, "text/html; charset=UTF-8", html);
}

void ConfigPortal::handleSystemPage(AsyncWebServerRequest *request) {
    String html = generateHeader("System", "system");
    
    html += "<div class='card'>";
    html += "<h2>System Actions</h2>";
    
    html += "<h3>Save & Restart</h3>";
    html += "<p>Apply all configuration changes and restart the device.</p>";
    html += "<form method='POST' action='/restart'>";
    html += "<button type='submit' class='btn btn-primary'>Save & Restart Now</button>";
    html += "</form>";
    
    html += "<hr>";
    
    html += "<h3>Factory Reset</h3>";
    html += "<p class='warning'>⚠ This will erase all configuration and restart the device!</p>";
    html += "<form method='POST' action='/factory-reset' onsubmit='return confirm(\"Are you sure? This will erase all settings!\")'>";
    html += "<button type='submit' class='btn btn-danger'>Factory Reset</button>";
    html += "</form>";
    
    html += "</div>";
    
    html += generateFooter();
    
    request->send(200, "text/html; charset=UTF-8", html);
}

void ConfigPortal::handleStatus(AsyncWebServerRequest *request) {
    DeviceConfiguration config;
    configManager.load(config);
    
    String json = "{";
    json += "\"configured\":" + String(config.configured ? "true" : "false") + ",";
    json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"wifi_ssid\":\"" + String(config.wifi_ssid) + "\",";
    json += "\"device_id\":\"" + String(config.device_id) + "\",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    
    request->send(200, "application/json", json);
}

void ConfigPortal::handleSaveConfig(AsyncWebServerRequest *request) {
    DeviceConfiguration config;
    configManager.load(config);
    
    String page = request->getParam("page", true)->value();
    
    // Update configuration based on which page submitted
    if (page == "wifi") {
        if (request->hasParam("wifi_ssid", true)) {
            String ssid = request->getParam("wifi_ssid", true)->value();
            ssid.toCharArray(config.wifi_ssid, sizeof(config.wifi_ssid));
        }
        if (request->hasParam("wifi_password", true)) {
            String password = request->getParam("wifi_password", true)->value();
            password.toCharArray(config.wifi_password, sizeof(config.wifi_password));
        }
    } else if (page == "device") {
        if (request->hasParam("device_id", true)) {
            String deviceId = request->getParam("device_id", true)->value();
            deviceId.toCharArray(config.device_id, sizeof(config.device_id));
        }
        if (request->hasParam("device_secret", true)) {
            String secret = request->getParam("device_secret", true)->value();
            secret.toCharArray(config.device_secret, sizeof(config.device_secret));
        }
        if (request->hasParam("door_id", true)) {
            String doorId = request->getParam("door_id", true)->value();
            doorId.toCharArray(config.door_id, sizeof(config.door_id));
        }
        if (request->hasParam("hardware_type", true)) {
            String hwType = request->getParam("hardware_type", true)->value();
            hwType.toCharArray(config.hardware_type, sizeof(config.hardware_type));
        }
    } else if (page == "api") {
        if (request->hasParam("api_base_url", true)) {
            String apiUrl = request->getParam("api_base_url", true)->value();
            apiUrl.toCharArray(config.api_base_url, sizeof(config.api_base_url));
        }
    }
    
    // Save configuration
    if (configManager.save(config)) {
        String html = generateHeader("Settings Saved", "");
        html += "<div class='card'>";
        html += "<h2>✓ Settings Saved Successfully</h2>";
        html += "<p>Your configuration has been saved.</p>";
        html += "<a href='/' class='btn'>Back to Dashboard</a> ";
        html += "<a href='/system' class='btn btn-primary'>Restart Device</a>";
        html += "</div>";
        html += generateFooter();
        request->send(200, "text/html; charset=UTF-8", html);
    } else {
        String html = generateHeader("Error", "");
        html += "<div class='card'>";
        html += "<h2>✗ Save Failed</h2>";
        html += "<p>Failed to save configuration. Please check your input and try again.</p>";
        html += "<a href='javascript:history.back()' class='btn'>Go Back</a>";
        html += "</div>";
        html += generateFooter();
        request->send(500, "text/html; charset=UTF-8", html);
    }
}

void ConfigPortal::handleFactoryReset(AsyncWebServerRequest *request) {
    String html = generateHeader("Factory Reset", "");
    html += "<div class='card'>";
    html += "<h2>Factory Reset in Progress</h2>";
    html += "<p>All configuration has been erased. Device will restart in 3 seconds...</p>";
    html += "<script>setTimeout(function(){ window.location.href='/'; }, 10000);</script>";
    html += "</div>";
    html += generateFooter();
    
    request->send(200, "text/html; charset=UTF-8", html);
    
    // Perform reset after response is sent
    delay(1000);
    configManager.factoryReset();
    delay(2000);
    ESP.restart();
}

void ConfigPortal::handleRestart(AsyncWebServerRequest *request) {
    String html = generateHeader("Restarting", "");
    html += "<div class='card'>";
    html += "<h2>Device Restarting</h2>";
    html += "<p>The device is restarting with the new configuration...</p>";
    html += "<p>If WiFi settings were changed, please reconnect to your network.</p>";
    html += "<script>setTimeout(function(){ window.location.href='/'; }, 15000);</script>";
    html += "</div>";
    html += generateFooter();
    
    request->send(200, "text/html; charset=UTF-8", html);
    
    // Restart after response is sent
    delay(2000);
    ESP.restart();
}



String ConfigPortal::generateHeader(const String& title, const String& activePage) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>" + title + " - ESP32 NFC</title>";
    html += "<style>";
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #f5f5f5; padding: 20px; }";
    html += ".container { max-width: 800px; margin: 0 auto; }";
    html += ".card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
    html += "h1, h2, h3 { color: #333; margin-bottom: 10px; }";
    html += ".subtitle { color: #666; font-size: 14px; }";
    html += "table { width: 100%; border-collapse: collapse; margin-top: 10px; }";
    html += "table td { padding: 8px; border-bottom: 1px solid #eee; }";
    html += "table td:first-child { font-weight: 500; color: #666; width: 40%; }";
    html += ".nav { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; }";
    html += ".nav a { padding: 10px 15px; background: white; border-radius: 5px; text-decoration: none; color: #333; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }";
    html += ".nav a.active { background: #007bff; color: white; }";
    html += ".form-group { margin-bottom: 15px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: 500; color: #333; }";
    html += "input[type='text'], input[type='password'], input[type='url'] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; }";
    html += "input:focus { outline: none; border-color: #007bff; }";
    html += "small { display: block; margin-top: 5px; color: #666; font-size: 12px; }";
    html += ".btn { display: inline-block; padding: 10px 20px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; font-size: 14px; }";
    html += ".btn:hover { background: #0056b3; }";
    html += ".btn-primary { background: #28a745; }";
    html += ".btn-primary:hover { background: #218838; }";
    html += ".btn-danger { background: #dc3545; }";
    html += ".btn-danger:hover { background: #c82333; }";
    html += ".warning { color: #dc3545; font-weight: 500; }";
    html += "hr { border: none; border-top: 1px solid #eee; margin: 20px 0; }";
    html += "</style>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += generateNavigation(activePage);
    return html;
}

String ConfigPortal::generateNavigation(const String& activePage) {
    String nav = "<div class='nav'>";
    
    nav += "<a href='/'";
    if (activePage == "home") nav += " class='active'";
    nav += ">Dashboard</a>";
    
    nav += "<a href='/wifi'";
    if (activePage == "wifi") nav += " class='active'";
    nav += ">WiFi</a>";
    
    nav += "<a href='/device'";
    if (activePage == "device") nav += " class='active'";
    nav += ">Device</a>";
    
    nav += "<a href='/api'";
    if (activePage == "api") nav += " class='active'";
    nav += ">API</a>";
    
    nav += "<a href='/system'";
    if (activePage == "system") nav += " class='active'";
    nav += ">System</a>";
    
    nav += "</div>";
    return nav;
}

String ConfigPortal::generateFooter() {
    String html = "<div class='card' style='text-align: center; color: #999; font-size: 12px;'>";
    html += "<p>ESP32 NFC Access Control System v" + String(FIRMWARE_VERSION) + "</p>";
    html += "</div>";
    html += "</div></body></html>";
    return html;
}
