#pragma once

#include <Arduino.h>

// Sleep-debt tracking.
//
// Standard definition: for each night, debt = max(0, need - actual), summed
// over a rolling window. Research converges on ~14 days as the window that
// matters for current impairment, and finds recent loss weighs more heavily
// than loss from two weeks ago -- so this reports BOTH the raw cumulative
// figure and a recency-weighted one.
//
// Grounding: Van Dongen et al. (2003) found 6 h/night for 14 days produced
// deficits equivalent to two nights of total sleep deprivation. Debt clears
// slowly: roughly 4 days of an extra hour to work off 10 hours of deficit.
//
// Caveat that belongs on the number itself: "actual sleep" here comes from
// contactless Cole-Kripke scoring, which runs ~78-80% against polysomnography
// and under-reports wake. So debt computed from it is an estimate built on an
// estimate. It is useful as a trend, not as a clinical figure.

#define DEBT_NIGHTS 14

struct NightSleep {
    uint16_t yday;         // day-of-year the night ended
    uint16_t totalMin;     // scored total sleep
    uint8_t  efficiency;
    uint8_t  valid;
};

class SleepDebt {
public:
    SleepDebt();

    void begin();
    void recordNight(uint16_t yday, uint16_t totalSleepMin, uint8_t efficiencyPct);

    int32_t cumulativeDebtMin() const;   // raw 14-night sum
    int32_t recentDebtMin() const;       // recency-weighted
    uint16_t nightsRecorded() const;
    uint16_t averageSleepMin() const;
    const NightSleep& night(uint8_t agoIdx) const;

    // How much the debt should delay the gentle wake, in minutes.
    // In debt -> let them sleep a bit longer before the first soft nudge.
    uint8_t wakeShiftMinutes() const;

    void clear();

private:
    void save();
    void load();

    NightSleep _n[DEBT_NIGHTS];
    uint8_t _head;
    uint8_t _count;
};

extern SleepDebt sleepDebt;
