#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"

enum NetMode {
    NET_BOOTING,
    NET_STA_CONNECTING,
    NET_STA_CONNECTED,
    NET_SETUP_AP
};

class Net {
public:
    Net();

    void begin();
    void loop();

    NetMode mode() const { return _mode; }
    const char* modeName() const;
    bool isConnected() const { return _mode == NET_STA_CONNECTED; }
    bool isSetupMode() const { return _mode == NET_SETUP_AP; }

    String ssid() const { return _ssid; }
    String ipAddress() const;
    int rssi() const;
    String hostname() const { return String(DEVICE_HOSTNAME) + ".local"; }

    bool saveCredentials(const String& ssid, const String& pass);
    void forgetCredentials();

    bool timeSynced() const { return _timeSynced; }
    bool localTime(struct tm* out) const;
    // Always returns a usable wall time even when NTP is offline.
    // If NTP never synced, synthesizes a free-running clock from boot millis
    // so the alarm still fires at the hard deadline (fail-safe, not fail-silent).
    bool timeForAlarm(struct tm* out, bool* trusted = nullptr) const;
    bool isTimeTrusted() const { return _timeSynced; }
    String timeString(const char* fmt = "%H:%M:%S") const;

private:
    void loadCredentials();
    void connectSTA();
    void startSetupAP();
    void startServices();

    NetMode _mode;
    String _ssid;
    String _pass;
    bool _timeSynced;
    bool _mdnsUp;
    uint32_t _connectStartedMs;
    uint32_t _lastRetryMs;
    uint32_t _bootMs;
    time_t _fallbackBaseEpoch;
};

extern Net net;
