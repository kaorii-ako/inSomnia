#pragma once

#include <Arduino.h>

// ── What this is, plainly ────────────────────────────────────────────────
// Two separate things live here, because they answer different questions:
//
// 1. classify() -- a CAUSAL 3-state machine (asleep / stirring / awake) with
//    hysteresis. It drives the alarm, so it may only look backwards. It is a
//    threshold heuristic. It is not a learned model and not "AI".
//
//    Its input is the thermal motion index, which is contactless video
//    actigraphy. That approach has been validated against polysomnography at
//    Cohen's kappa 0.733 -- comparable to a wrist actigraph.
//
// 2. coleKripke() -- the published Cole-Kripke (1992) sleep/wake rule, used
//    for the morning report only. It needs the two FOLLOWING epochs, so it
//    lags 2 minutes and can never drive a real-time decision.
//
// Cole-Kripke is validated at roughly 78-80% sleep/wake agreement with
// polysomnography, but its specificity for WAKE is only 0.35-0.64 -- it is
// much better at spotting sleep than at spotting wakefulness. That is exactly
// why alarm suppression is driven by outOfBed() (a blunt near-binary signal)
// and not by this classifier.
//
// The weights below assume ActiGraph counts. Ours are arbitrary units, so
// Settings::countScale is a calibration constant you must tune against your
// own bed. The algorithm's shape is validated; this count scale is not.

#define EPOCH_SECONDS   60
#define EPOCH_RING      720    // 12 hours

enum SleepState {
    STATE_UNKNOWN = 0,
    STATE_ASLEEP,
    STATE_STIRRING,
    STATE_AWAKE
};

struct NightStats {
    uint16_t epochsInBed;
    uint16_t epochsAsleep;
    uint16_t sleepOnsetLatencyMin;   // SOL
    uint16_t wakeAfterSleepOnsetMin; // WASO
    uint16_t totalSleepMin;          // TST
    uint8_t  sleepEfficiencyPct;     // TST / time in bed
    uint16_t stirringEpisodes;
    bool     valid;
};

class SleepModel {
public:
    SleepModel();

    void begin();
    // Called once per epoch with that epoch's fused activity count.
    void pushEpoch(uint16_t counts, float micLoudFraction, uint8_t inBedPercent);

    SleepState state() const { return _state; }
    const char* stateName() const;
    uint32_t secondsInState() const;

    bool outOfBed() const { return _outOfBedEpochs >= OUT_OF_BED_CONFIRM; }
    uint16_t lastCounts() const { return _lastCounts; }
    uint16_t epochCount() const { return _count; }

    bool degraded() const { return _degraded; }
    void setDegraded(bool d) { _degraded = d; }

    NightStats computeNightStats() const;
    uint16_t countsAt(uint16_t epochsAgo) const;

    void resetNight();

private:
    static const uint8_t OUT_OF_BED_CONFIRM = 3;

    bool coleKripkeSleepAt(uint16_t idx) const;
    void classify(uint16_t counts, float micLoudFraction, uint8_t inBedPercent);

    uint16_t _ring[EPOCH_RING];
    uint16_t _head;
    uint16_t _count;

    SleepState _state;
    uint32_t _stateEnteredMs;
    uint8_t _aboveAwake;
    uint8_t _belowAwake;
    uint8_t _aboveStirring;
    uint8_t _belowStirring;
    uint8_t _outOfBedEpochs;
    uint16_t _lastCounts;
    bool _degraded;
};

extern SleepModel sleepModel;
