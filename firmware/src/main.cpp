// inSomnia — adaptive wake clock
// Phases 1-4: hardware scaffold, network, sensing, sleep-state, alarm logic.

#include <Arduino.h>
#include "config.h"
#include "pin_config.h"
#include "settings.h"
#include "display.h"
#include "buzzer.h"
#include "key_matrix.h"
#include "thermal.h"
#include "mic.h"
#include "sleep_debt.h"
#include "sleep_model.h"
#include "alarm.h"
#include "net.h"
#include "web_ui.h"

#define COL_BG      ST77XX_BLACK
#define COL_DIM     0x4A69
#define COL_TEXT    ST77XX_WHITE
#define COL_ACCENT  0x4D3F
#define COL_OK      0x2FEB
#define COL_WARN    ST77XX_YELLOW
#define COL_ALERT   ST77XX_RED

#define KEY_DISMISS  0   // Key 1
#define KEY_SNOOZE   1   // Key 2

static uint32_t lastRenderMs = 0;
static uint32_t lastEpochMs = 0;
static char cacheTime[16] = "";
static char cacheState[28] = "";
static char cacheAlarm[28] = "";
static char cacheNet[28] = "";
static char cacheSens[28] = "";

static void runEpoch() {
    uint8_t inBedPct = thermal.epochInBedPercent();
    thermal.rollEpoch();
    mic.rollEpoch();

    float loud = mic.lastEpochLoudFraction();

    uint32_t c = thermal.lastEpochCounts();
    uint16_t counts = (c > 65535) ? 65535 : (uint16_t)c;

    sleepModel.pushEpoch(counts, loud, inBedPct);

    Serial.printf("[epoch] counts=%u loud=%.2f inbed=%u%% warm=%u state=%s oob=%d\n",
                  counts, loud, inBedPct, thermal.warmPixels(),
                  sleepModel.stateName(), sleepModel.outOfBed() ? 1 : 0);
}

static void drawRow(int16_t y, const char* text, uint16_t color,
                    char* cache, size_t cacheLen) {
    if (strncmp(cache, text, cacheLen) == 0) return;
    strncpy(cache, text, cacheLen - 1);
    cache[cacheLen - 1] = '\0';
    display.fillRow(y, 10, COL_BG);
    display.drawText(text, 4, y, color, 1);
}

static void render() {
    struct tm t;
    bool synced = net.localTime(&t);

    char timeBuf[16];
    if (synced) strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &t);
    else        snprintf(timeBuf, sizeof(timeBuf), "--:--:--");

    if (strcmp(cacheTime, timeBuf) != 0) {
        strncpy(cacheTime, timeBuf, sizeof(cacheTime) - 1);
        display.fillRow(18, 24, COL_BG);
        display.drawTextCentered(timeBuf, 18, synced ? COL_TEXT : COL_DIM, 3);
    }

    char buf[32];
    uint16_t stateCol = COL_DIM;
    switch (sleepModel.state()) {
        case STATE_ASLEEP:   stateCol = COL_OK; break;
        case STATE_STIRRING: stateCol = COL_WARN; break;
        case STATE_AWAKE:    stateCol = COL_ACCENT; break;
        default: break;
    }
    snprintf(buf, sizeof(buf), "%-8s  %5u", sleepModel.stateName(),
             sleepModel.lastCounts());
    drawRow(50, buf, stateCol, cacheState, sizeof(cacheState));

    AlarmPhase ph = alarmEngine.phase();
    int32_t mins = (alarmEngine.minutesToWindowOpen() > 0)
                 ? alarmEngine.minutesToWindowOpen()
                 : alarmEngine.minutesToDeadline();
    const char* lbl = (alarmEngine.minutesToWindowOpen() > 0) ? "open" : "dline";
    snprintf(buf, sizeof(buf), "%-10s %s %ldm",
             alarmEngine.phaseName(), lbl, (long)mins);
    drawRow(62, buf,
            (ph == ALARM_HARD) ? COL_ALERT :
            (ph == ALARM_GENTLE) ? COL_WARN : COL_DIM,
            cacheAlarm, sizeof(cacheAlarm));

    snprintf(buf, sizeof(buf), "th%c mic%c %s%s",
             thermal.healthy() ? '+' : '-',
             mic.healthy() ? '+' : '-',
             thermal.inBed() ? "inbed" : "empty",
             alarmEngine.failsafeActive() ? " FAIL" : "");
    drawRow(80, buf,
            alarmEngine.failsafeActive() ? COL_ALERT : COL_DIM,
            cacheSens, sizeof(cacheSens));

    snprintf(buf, sizeof(buf), "%s", net.isSetupMode()
             ? SETUP_AP_SSID : net.ipAddress().c_str());
    drawRow(92, buf, COL_DIM, cacheNet, sizeof(cacheNet));
}

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("inSomnia " FW_VERSION);

    settings.begin();
    display.begin();
    display.setBrightness(settings.get().backlight);
    buzzer.begin();
    keys.begin();
    thermal.begin();
    mic.begin();
    sleepModel.begin();
    sleepDebt.begin();

    {
        const Settings& c = settings.get();
        thermal.setRoi(c.roiX0, c.roiY0, c.roiX1, c.roiY1);
    }

    net.begin();
    webui.begin();
    alarmEngine.begin();

    display.clear(COL_BG);
    display.drawText("inSomnia", 4, 4, COL_ACCENT, 1);
    display.tft().drawFastHLine(0, 15, display.width(), 0x2124);
    display.tft().drawFastHLine(0, 46, display.width(), 0x2124);
    display.tft().drawFastHLine(0, 76, display.width(), 0x2124);
    display.drawText("K1 dismiss  K2 snooze", 4, 112, COL_DIM, 1);

    lastEpochMs = millis();
    Serial.printf("[boot] heap %u, thermal=%d mic=%d, debt %ld min over %u nights\n",
                  ESP.getFreeHeap(), thermal.present(), mic.present(),
                  (long)sleepDebt.cumulativeDebtMin(), sleepDebt.nightsRecorded());
}

void loop() {
    net.loop();
    webui.loop();
    keys.update();
    thermal.update();
    mic.update();

    if (keys.wasJustPressed(KEY_DISMISS)) alarmEngine.dismiss();
    if (keys.wasJustPressed(KEY_SNOOZE))  alarmEngine.snooze();

    uint32_t now = millis();
    if (now - lastEpochMs >= (uint32_t)EPOCH_SECONDS * 1000UL) {
        lastEpochMs = now;
        runEpoch();
    }

    alarmEngine.update();

    if (now - lastRenderMs >= 250) {
        lastRenderMs = now;
        render();
    }
}
