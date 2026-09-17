#include "net.h"
#include <ESPmDNS.h>
#include <Preferences.h>

Net net;

static Preferences prefs;

Net::Net()
    : _mode(NET_BOOTING), _timeSynced(false), _mdnsUp(false),
      _connectStartedMs(0), _lastRetryMs(0), _bootMs(0), _fallbackBaseEpoch(0) {
    // Fallback base: 2024-01-01 06:00:00 local (fake, advances via millis).
    // Used only when NTP never syncs — ensures alarm still fires at deadline.
    struct tm base = {};
    base.tm_year = 2024 - 1900;
    base.tm_mon = 0;
    base.tm_mday = 1;
    base.tm_hour = 6;
    base.tm_min = 0;
    base.tm_sec = 0;
    base.tm_isdst = -1;
    _fallbackBaseEpoch = mktime(&base);
    if (_fallbackBaseEpoch < 0) _fallbackBaseEpoch = 1704088800;
}

void Net::loadCredentials() {
    prefs.begin(NVS_NAMESPACE, true);
    _ssid = prefs.getString("ssid", "");
    _pass = prefs.getString("pass", "");
    prefs.end();
}

void Net::begin() {
    _bootMs = millis();
    loadCredentials();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    if (_ssid.length() == 0) {
        Serial.println("[net] no stored credentials, starting setup AP");
        startSetupAP();
    } else {
        connectSTA();
    }
}

void Net::connectSTA() {
    Serial.printf("[net] connecting to \"%s\"\n", _ssid.c_str());
    _mode = NET_STA_CONNECTING;
    _timeSynced = false;

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.begin(_ssid.c_str(), _pass.c_str());
    _connectStartedMs = millis();
}

void Net::startSetupAP() {
    Serial.println("[net] setup AP up: " SETUP_AP_SSID " -> http://192.168.4.1");
    _mode = NET_SETUP_AP;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASS);
}

void Net::startServices() {
    if (!_mdnsUp && MDNS.begin(DEVICE_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        _mdnsUp = true;
        Serial.printf("[net] mDNS: http://%s.local\n", DEVICE_HOSTNAME);
    }
    configTzTime(TZ_STRING, NTP_SERVER_1, NTP_SERVER_2);
    Serial.println("[net] NTP requested");
}

void Net::loop() {
    uint32_t now = millis();

    switch (_mode) {
        case NET_STA_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                _mode = NET_STA_CONNECTED;
                Serial.printf("[net] connected, ip=%s rssi=%d\n",
                              WiFi.localIP().toString().c_str(), WiFi.RSSI());
                startServices();
            } else if (now - _connectStartedMs > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.println("[net] connect timed out, falling back to setup AP");
                startSetupAP();
            }
            break;

        case NET_STA_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[net] link lost");
                _mode = NET_STA_CONNECTING;
                _connectStartedMs = now;
                WiFi.reconnect();
            } else if (!_timeSynced) {
                struct tm t;
                if (getLocalTime(&t, 0) && t.tm_year > (2020 - 1900)) {
                    _timeSynced = true;
                    Serial.printf("[net] time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                                  t.tm_hour, t.tm_min, t.tm_sec);
                }
            }
            break;

        case NET_SETUP_AP:
            if (_ssid.length() > 0 && now - _lastRetryMs > WIFI_RETRY_INTERVAL_MS) {
                _lastRetryMs = now;
                connectSTA();
            }
            break;

        default:
            break;
    }
}

const char* Net::modeName() const {
    switch (_mode) {
        case NET_BOOTING:        return "booting";
        case NET_STA_CONNECTING: return "connecting";
        case NET_STA_CONNECTED:  return "online";
        case NET_SETUP_AP:       return "setup-ap";
    }
    return "?";
}

String Net::ipAddress() const {
    if (_mode == NET_SETUP_AP) return WiFi.softAPIP().toString();
    if (_mode == NET_STA_CONNECTED) return WiFi.localIP().toString();
    return String("0.0.0.0");
}

int Net::rssi() const {
    return (_mode == NET_STA_CONNECTED) ? WiFi.RSSI() : 0;
}

bool Net::saveCredentials(const String& ssid, const String& pass) {
    if (ssid.length() == 0) return false;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
    _ssid = ssid;
    _pass = pass;
    _lastRetryMs = 0;
    return true;
}

void Net::forgetCredentials() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
    _ssid = "";
    _pass = "";
}

bool Net::localTime(struct tm* out) const {
    if (!_timeSynced) return false;
    return getLocalTime(out, 0);
}

bool Net::timeForAlarm(struct tm* out, bool* trusted) const {
    if (trusted) *trusted = _timeSynced;
    if (_timeSynced && getLocalTime(out, 0) && out->tm_year > (2020 - 1900)) {
        return true;
    }
    // Fallback: free-running clock from boot. Advances via millis() so the
    // hard deadline always arrives even fully offline.
    if (!out) return false;
    uint32_t elapsedSec = (millis() - _bootMs) / 1000;
    time_t est = _fallbackBaseEpoch + (time_t)elapsedSec;
    // Apply TZ offset manually via localtime_r on the estimated epoch.
    // Use localtime_r which honors TZ env set by configTzTime earlier.
    struct tm* tp = localtime_r(&est, out);
    if (!tp) {
        // ultra-fallback: synthesize directly
        out->tm_year = 2024 - 1900;
        out->tm_mon = 0;
        out->tm_mday = 1;
        out->tm_hour = (6 + (elapsedSec / 3600)) % 24;
        out->tm_min = (elapsedSec / 60) % 60;
        out->tm_sec = elapsedSec % 60;
        out->tm_yday = 0;
        out->tm_isdst = -1;
    }
    // yday needs correct value even on synthetic path
    if (out->tm_yday < 0) out->tm_yday = 0;
    return true;
}

String Net::timeString(const char* fmt) const {
    struct tm t;
    bool trusted = false;
    if (!timeForAlarm(&t, &trusted)) return String("--:--:--");
    char buf[32];
    strftime(buf, sizeof(buf), fmt, &t);
    if (!trusted) {
        // indicate estimate
        String s(buf);
        return s + "*";
    }
    return String(buf);
}
