#pragma once

#include <Arduino.h>
#include <WebServer.h>

class WebUI {
public:
    WebUI();

    void begin();
    void loop();

private:
    void routes();
    void handleRoot();
    void handleState();
    void handleThermal();
    void handleGetSettings();
    void handleSetSettings();
    void handleAlarm();
    void handleScan();
    void handleWifiSave();
    void handleForget();
    void handleBuzzer();
    void handleBacklight();

    WebServer _server;
    bool _rebootPending;
    uint32_t _rebootAtMs;
};

extern WebUI webui;
