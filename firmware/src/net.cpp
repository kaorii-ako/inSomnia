#include "net.h"
#include <ESPmDNS.h>
#include <Preferences.h>

Net net;

static Preferences prefs;

Net::Net()
    : _mode(NET_BOOTING), _timeSynced(false), _mdnsUp(false),
      _connectStartedMs(0), _lastRetryMs(0) {}

void Net::loadCredentials() {
    prefs.begin(NVS_NAMESPACE, true);
    _ssid = prefs.getString("ssid", "");
    _pass = prefs.getString("pass", "");
    prefs.end();
}

void Net::begin() {
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

String Net::timeString(const char* fmt) const {
    struct tm t;
    if (!localTime(&t)) return String("--:--:--");
    char buf[32];
    strftime(buf, sizeof(buf), fmt, &t);
    return String(buf);
}
