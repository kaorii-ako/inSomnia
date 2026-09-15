#include "settings.h"
#include "config.h"
#include <Preferences.h>

SettingsStore settings;
static Preferences prefs;

void Settings::setDefaults() {
    windowOpenMin      = 6 * 60 + 50;   // 06:50
    windowDeadlineMin  = 7 * 60 + 20;   // 07:20
    alarmEnabled       = true;
    suppressEnabled    = true;

    // A duvet attenuates skin temperature hard, so 2.0 degC above the coolest
    // pixel is a deliberately low bar. Tune it against your own bed.
    bodyMarginTenthC   = 20;
    inBedMinPixels     = 25;
    roiX0 = 0; roiY0 = 0; roiX1 = 31; roiY1 = 23;

    countScale         = 100;
    stirringCounts     = 40;
    awakeCounts        = 180;
    awakeConfirmEpochs = 3;
    micStirringRms     = 900;

    sleepNeedMin       = 8 * 60;
    maxDebtShiftMin    = 15;
    debtAwareEnabled   = true;

    gentleStartVol     = 30;
    gentleRampMin      = 8;
    hardVol            = 255;
    snoozeMin          = 7;
    backlight          = 40;
}

void SettingsStore::begin() {
    _s.setDefaults();
    prefs.begin(NVS_NAMESPACE, true);
    if (prefs.isKey("cfg")) {
        Settings tmp;
        size_t n = prefs.getBytes("cfg", &tmp, sizeof(tmp));
        if (n == sizeof(tmp)) _s = tmp;
    }
    prefs.end();
}

bool SettingsStore::save() {
    prefs.begin(NVS_NAMESPACE, false);
    size_t n = prefs.putBytes("cfg", &_s, sizeof(_s));
    prefs.end();
    return n == sizeof(_s);
}

void SettingsStore::resetDefaults() {
    _s.setDefaults();
    save();
}
