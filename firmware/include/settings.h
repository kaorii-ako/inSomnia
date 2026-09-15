#pragma once

#include <Arduino.h>

struct Settings {
    // ── wake window ──────────────────────────────────────────────
    uint16_t windowOpenMin;      // minutes past midnight, earliest wake
    uint16_t windowDeadlineMin;  // minutes past midnight, hard deadline
    bool     alarmEnabled;
    bool     suppressEnabled;    // skip alarm if already out of bed

    // ── thermal sensing ──────────────────────────────────────────
    uint16_t bodyMarginTenthC;   // pixel counts as body if > ambient + this/10 degC
    uint16_t inBedMinPixels;     // warm pixels in ROI needed to call it "in bed"
    uint8_t  roiX0, roiY0, roiX1, roiY1;   // bed region in the 32x24 frame

    // ── sleep/wake model ─────────────────────────────────────────
    uint16_t countScale;         // motion counts -> Cole-Kripke units (calibration)
    uint16_t stirringCounts;
    uint16_t awakeCounts;
    uint8_t  awakeConfirmEpochs;
    uint16_t micStirringRms;

    // ── sleep debt ───────────────────────────────────────────────
    uint16_t sleepNeedMin;       // nightly target, minutes (default 8h)
    uint8_t  maxDebtShiftMin;    // how much debt may delay the gentle wake
    bool     debtAwareEnabled;

    // ── alert ────────────────────────────────────────────────────
    uint8_t  gentleStartVol;
    uint8_t  gentleRampMin;
    uint8_t  hardVol;
    uint8_t  snoozeMin;
    uint8_t  backlight;

    void setDefaults();
};

class SettingsStore {
public:
    void begin();
    bool save();
    void resetDefaults();
    Settings& get() { return _s; }
    const Settings& get() const { return _s; }
private:
    Settings _s;
};

extern SettingsStore settings;
