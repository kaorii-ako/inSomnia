#pragma once

#define FW_VERSION          "0.1.0-phase1"
#define DEVICE_HOSTNAME     "insomnia"
#define SETUP_AP_SSID       "inSomnia-setup"
#define SETUP_AP_PASS       "insomnia"

#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WIFI_RETRY_INTERVAL_MS   30000

#define NTP_SERVER_1   "pool.ntp.org"
#define NTP_SERVER_2   "time.google.com"

// POSIX TZ string. Asia/Bangkok has no DST. Change here for another zone,
// e.g. "GMT0BST,M3.5.0/1,M10.5.0" for UK, "EST5EDT,M3.2.0,M11.1.0" for US East.
#define TZ_STRING      "ICT-7"

#define NVS_NAMESPACE  "insomnia"
