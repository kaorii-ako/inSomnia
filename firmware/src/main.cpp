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

// ── Keymap ───────────────────────────────────────────────────────────────
// The board carries 12 keys in a 4x3 grid (inherited from the original
// schematic). Layout matches the enclosure: bit 0 is front-left.
//
//   row 1  DISMISS   SNOOZE    LIGHT     SKIP
//   row 2  OPEN -    OPEN +    DEAD -    DEAD +
//   row 3  PAGE      TEST SOFT TEST LOUD SAVE
//
// Row 1 is what you hit half-asleep; rows 2 and 3 are setup you do awake.
#define KEY_DISMISS    0
#define KEY_SNOOZE     1
#define KEY_LIGHT      2
#define KEY_SKIP       3
#define KEY_OPEN_DN    4
#define KEY_OPEN_UP    5
#define KEY_DEAD_DN    6
#define KEY_DEAD_UP    7
#define KEY_PAGE       8
#define KEY_TEST_SOFT  9
#define KEY_TEST_LOUD  10
#define KEY_SAVE       11

enum UiPage { PAGE_STATUS, PAGE_WINDOW, PAGE_DEBT, PAGE_COUNT_ };
static UiPage uiPage = PAGE_STATUS;
static uint32_t toastUntilMs = 0;
static char toast[24] = "";

static void showToast(const char* t) {
    strncpy(toast, t, sizeof(toast)-1);
    toast[sizeof(toast)-1] = 0;
    toastUntilMs = millis() + 1600;
}

static uint16_t clampMin(int32_t v) {
    while (v < 0) v += 1440;
    return (uint16_t)(v % 1440);
}

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

static bool forceRedraw = false;

static void renderWindowPage() {
    const Settings& c = settings.get();
    char buf[32];
    snprintf(buf, sizeof(buf), "open   %02u:%02u",
             c.windowOpenMin/60, c.windowOpenMin%60);
    drawRow(50, buf, COL_TEXT, cacheState, sizeof(cacheState));
    snprintf(buf, sizeof(buf), "dline  %02u:%02u",
             c.windowDeadlineMin/60, c.windowDeadlineMin%60);
    drawRow(62, buf, COL_TEXT, cacheAlarm, sizeof(cacheAlarm));
    snprintf(buf, sizeof(buf), "debt shift +%um", alarmEngine.debtShiftMinutes());
    drawRow(80, buf, COL_DIM, cacheSens, sizeof(cacheSens));
    drawRow(92, "K12 saves", COL_DIM, cacheNet, sizeof(cacheNet));
}

static void renderDebtPage() {
    char buf[32];
    int32_t cum = sleepDebt.cumulativeDebtMin();
    snprintf(buf, sizeof(buf), "debt  %ldh%02ldm", (long)(cum/60), (long)(cum%60));
    drawRow(50, buf, cum > 120 ? COL_WARN : COL_OK, cacheState, sizeof(cacheState));
    uint16_t avg = sleepDebt.averageSleepMin();
    snprintf(buf, sizeof(buf), "avg   %uh%02um", avg/60, avg%60);
    drawRow(62, buf, COL_DIM, cacheAlarm, sizeof(cacheAlarm));
    snprintf(buf, sizeof(buf), "nights %u/14", sleepDebt.nightsRecorded());
    drawRow(80, buf, COL_DIM, cacheSens, sizeof(cacheSens));
    const NightRecord& n = alarmEngine.lastNight();
    snprintf(buf, sizeof(buf), "last  %uh%02um", n.totalSleepMin/60, n.totalSleepMin%60);
    drawRow(92, buf, COL_DIM, cacheNet, sizeof(cacheNet));
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

    if (forceRedraw) {
        forceRedraw = false;
        cacheState[0] = cacheAlarm[0] = cacheSens[0] = cacheNet[0] = '\x01';
        display.fillRow(48, 58, COL_BG);
    }

    if (millis() < toastUntilMs) {
        display.fillRow(100, 10, COL_BG);
        display.drawText(toast, 4, 100, COL_ACCENT, 1);
    } else if (toast[0]) {
        display.fillRow(100, 10, COL_BG);
        toast[0] = 0;
    }

    if (uiPage == PAGE_WINDOW) { renderWindowPage(); return; }
    if (uiPage == PAGE_DEBT)   { renderDebtPage();   return; }

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
    display.drawText("K1 off K2 snz K9 page", 4, 112, COL_DIM, 1);

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

    Settings& cfg = settings.get();
    if (keys.wasJustPressed(KEY_DISMISS)) { alarmEngine.dismiss(); showToast("dismissed"); }
    if (keys.wasJustPressed(KEY_SNOOZE))  { alarmEngine.snooze();  showToast("snoozed"); }
    if (keys.wasJustPressed(KEY_LIGHT)) {
        static const uint8_t steps[] = {0, 20, 60, 140, 255};
        uint8_t i = 0;
        for (uint8_t k = 0; k < 5; k++) if (cfg.backlight >= steps[k]) i = k;
        cfg.backlight = steps[(i + 1) % 5];
        display.setBrightness(cfg.backlight);
        showToast("light");
    }
    if (keys.wasJustPressed(KEY_SKIP))   { alarmEngine.skipTonight(); showToast("skip tonight"); }

    if (keys.wasJustPressed(KEY_OPEN_DN)) { cfg.windowOpenMin = clampMin((int32_t)cfg.windowOpenMin - 5); uiPage = PAGE_WINDOW; }
    if (keys.wasJustPressed(KEY_OPEN_UP)) { cfg.windowOpenMin = clampMin((int32_t)cfg.windowOpenMin + 5); uiPage = PAGE_WINDOW; }
    if (keys.wasJustPressed(KEY_DEAD_DN)) { cfg.windowDeadlineMin = clampMin((int32_t)cfg.windowDeadlineMin - 5); uiPage = PAGE_WINDOW; }
    if (keys.wasJustPressed(KEY_DEAD_UP)) { cfg.windowDeadlineMin = clampMin((int32_t)cfg.windowDeadlineMin + 5); uiPage = PAGE_WINDOW; }

    if (keys.wasJustPressed(KEY_PAGE))      { uiPage = (UiPage)((uiPage + 1) % PAGE_COUNT_); forceRedraw = true; }
    if (keys.wasJustPressed(KEY_TEST_SOFT)) { alarmEngine.testGentle(); showToast("test: soft"); }
    if (keys.wasJustPressed(KEY_TEST_LOUD)) {
        if (alarmEngine.phase() == ALARM_HARD || alarmEngine.phase() == ALARM_GENTLE) {
            alarmEngine.stopTest(); showToast("test stopped");
        } else { alarmEngine.testHard(); showToast("test: loud"); }
    }
    if (keys.wasJustPressed(KEY_SAVE))      { settings.save(); showToast("saved"); }

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
