#pragma once

#include <Arduino.h>
#include "sleep_model.h"

enum AlarmPhase {
    ALARM_IDLE,        // outside the night cycle / disabled
    ALARM_ARMED,       // waiting for the window
    ALARM_SUPPRESSED,  // you were already up before the window opened
    ALARM_GENTLE,      // stirring detected inside the window, ramping up
    ALARM_HARD,        // deadline reached, full intensity
    ALARM_SNOOZED,
    ALARM_DONE
};

enum NightOutcome {
    OUTCOME_NONE = 0,
    OUTCOME_SUPPRESSED,     // skipped, you were already up
    OUTCOME_STIRRED_WOKE,   // gentle wake caught you stirring
    OUTCOME_HARD_WOKE,      // slept to the deadline
    OUTCOME_FAILSAFE        // sensors were down, fixed-time alarm fired
};

struct NightRecord {
    uint32_t   epochStamp;
    uint8_t    outcome;
    uint8_t    wokeHour;
    uint8_t    wokeMin;
    uint8_t    sleepEfficiencyPct;
    uint16_t   totalSleepMin;
    uint16_t   wasoMin;
    uint16_t   solMin;
};

class AlarmEngine {
public:
    AlarmEngine();

    void begin();
    void update();

    void dismiss();
    void snooze();
    void skipTonight();
    void testGentle();
    void testHard();
    void stopTest();

    AlarmPhase phase() const { return _phase; }
    const char* phaseName() const;
    NightOutcome outcome() const { return _outcome; }
    const char* outcomeName() const;
    static const char* outcomeNameOf(uint8_t o);

    int32_t minutesToWindowOpen() const { return _minsToOpen; }
    int32_t minutesToDeadline() const { return _minsToDeadline; }
    bool failsafeActive() const { return _failsafe; }
    uint8_t debtShiftMinutes() const { return _debtShift; }
    uint8_t currentVolume() const { return _volume; }

    const NightRecord& lastNight() const { return _lastNight; }
    void saveLastNight();
    void loadLastNight();

private:
    void enter(AlarmPhase p);
    void driveOutput();
    void recordOutcome(NightOutcome o);
    bool sensorsUsable() const;

    AlarmPhase _phase;
    NightOutcome _outcome;
    bool _failsafe;
    uint8_t _volume;
    uint32_t _phaseEnteredMs;
    uint32_t _lastBeepMs;
    bool _beepOn;
    int32_t _minsToOpen;
    int32_t _minsToDeadline;
    int _lastYday;
    uint16_t _snoozeUntilMin;
    bool _testMode;
    uint8_t _debtShift;
    NightRecord _lastNight;
};

extern AlarmEngine alarmEngine;
