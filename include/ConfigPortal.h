#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "ConfigManager.h"

class ConfigPortal {
public:
    ConfigPortal(ConfigManager& configManager);
    
    void begin();
    
    void stop();
    
    bool isActive();
    
    void update();
    
    String getAPSSID();
    
private:
    ConfigManager& configManager;
    AsyncWebServer server;
    DNSServer dnsServer;
    bool active;
    String apSSID;
    
    void generateAPSSID();
    
    void setupRoutes();
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest *request);
    void handleWiFiPage(AsyncWebServerRequest *request);
    void handleDevicePage(AsyncWebServerRequest *request);
    void handleAPIPage(AsyncWebServerRequest *request);
    void handleSystemPage(AsyncWebServerRequest *request);
    void handleStatus(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request);
    void handleFactoryReset(AsyncWebServerRequest *request);
    void handleRestart(AsyncWebServerRequest *request);
    
    // HTML template generators
    String generateHeader(const String& title, const String& activePage);
    String generateFooter();
    String generateNavigation(const String& activePage);
};

#endif
