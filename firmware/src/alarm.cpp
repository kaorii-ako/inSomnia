#include "alarm.h"
#include "settings.h"
#include "buzzer.h"
#include "net.h"
#include "thermal.h"
#include "mic.h"
#include "sleep_debt.h"
#include "config.h"
#include <Preferences.h>

AlarmEngine alarmEngine;
static Preferences aprefs;

#define GENTLE_TONE_HZ   880
#define HARD_TONE_HZ     3200
#define GENTLE_PERIOD_MS 2600
#define GENTLE_BEEP_MS   420
#define HARD_PERIOD_MS   700
#define HARD_BEEP_MS     380
#define SUPPRESS_CONFIRM_SEC 120

AlarmEngine::AlarmEngine()
    : _phase(ALARM_IDLE), _outcome(OUTCOME_NONE), _failsafe(false), _volume(0),
      _phaseEnteredMs(0), _lastBeepMs(0), _beepOn(false), _minsToOpen(0),
      _minsToDeadline(0), _lastYday(-1), _snoozeUntilMin(0), _testMode(false), _debtShift(0) {
    memset(&_lastNight, 0, sizeof(_lastNight));
}

void AlarmEngine::begin() {
    loadLastNight();
    enter(ALARM_ARMED);
}

void AlarmEngine::enter(AlarmPhase p) {
    if (_phase == p) return;
    _phase = p;
    _phaseEnteredMs = millis();
    _beepOn = false;
    if (p != ALARM_GENTLE && p != ALARM_HARD) {
        _volume = 0;
        buzzer.stop();
    }
    Serial.printf("[alarm] -> %s\n", phaseName());
}

bool AlarmEngine::sensorsUsable() const {
    // The thermal array is the load-bearing sensor. The mic alone cannot tell
    // whether you are in the bed, so it is not a substitute.
    return thermal.healthy();
}

void AlarmEngine::update() {
    const Settings& cfg = settings.get();

    struct tm t;
    if (!net.localTime(&t)) {
        // No trusted clock yet: never fire, never claim suppression.
        _failsafe = true;
        if (_phase == ALARM_GENTLE || _phase == ALARM_HARD) driveOutput();
        return;
    }

    uint16_t nowMin = t.tm_hour * 60 + t.tm_min;

    if (_lastYday != t.tm_yday) {
        _lastYday = t.tm_yday;
        _outcome = OUTCOME_NONE;
        _snoozeUntilMin = 0;
        _testMode = false;
        sleepModel.resetNight();
        enter(ALARM_ARMED);
        Serial.println("[alarm] new day, re-armed");
    }

    // Sleep-debt-aware waking: when you are carrying debt, hold the gentle
    // wake back a little so an early stir does not cost you the lie-in. The
    // hard deadline never moves -- debt can delay the nudge, never the alarm.
    _debtShift = sleepDebt.wakeShiftMinutes();
    uint16_t effectiveOpen = cfg.windowOpenMin + _debtShift;
    if (effectiveOpen > cfg.windowDeadlineMin) effectiveOpen = cfg.windowDeadlineMin;

    _minsToOpen = (int32_t)effectiveOpen - (int32_t)nowMin;
    _minsToDeadline = (int32_t)cfg.windowDeadlineMin - (int32_t)nowMin;

    // Fail SAFE, never fail silent: if the sensors are gone we stop trusting
    // stirring detection and suppression, and degrade to a plain fixed-time
    // alarm at the hard deadline.
    _failsafe = !sensorsUsable();
    sleepModel.setDegraded(_failsafe);

    if (_testMode) { driveOutput(); return; }

    if (!cfg.alarmEnabled) {
        if (_phase != ALARM_IDLE) enter(ALARM_IDLE);
        return;
    }

    switch (_phase) {
        case ALARM_IDLE:
            enter(ALARM_ARMED);
            break;

        case ALARM_ARMED:
            if (_minsToOpen > 0) {
                // Before the window: suppress only on the blunt out-of-bed
                // signal, never on the sleep classifier alone.
                if (cfg.suppressEnabled && !_failsafe &&
                    sleepModel.outOfBed() &&
                    sleepModel.state() == STATE_AWAKE &&
                    sleepModel.secondsInState() >= SUPPRESS_CONFIRM_SEC) {
                    recordOutcome(OUTCOME_SUPPRESSED);
                    enter(ALARM_SUPPRESSED);
                }
            } else if (_minsToDeadline > 0) {
                // Inside the window.
                if (!_failsafe && (sleepModel.state() == STATE_STIRRING ||
                                   sleepModel.state() == STATE_AWAKE)) {
                    enter(ALARM_GENTLE);
                }
            } else {
                enter(ALARM_HARD);
            }
            break;

        case ALARM_SUPPRESSED:
            // Stays suppressed for the rest of this day's window.
            if (_minsToDeadline < 0) enter(ALARM_DONE);
            break;

        case ALARM_GENTLE:
            if (_minsToDeadline <= 0) enter(ALARM_HARD);
            break;

        case ALARM_HARD:
            break;

        case ALARM_SNOOZED:
            if (nowMin >= _snoozeUntilMin) enter(ALARM_HARD);
            break;

        case ALARM_DONE:
            break;
    }

    driveOutput();
}

void AlarmEngine::driveOutput() {
    const Settings& cfg = settings.get();
    uint32_t now = millis();

    if (_phase != ALARM_GENTLE && _phase != ALARM_HARD) {
        if (_beepOn) { buzzer.stop(); _beepOn = false; }
        return;
    }

    uint16_t tone;
    uint32_t period, onMs;

    if (_phase == ALARM_GENTLE) {
        uint32_t elapsedMin = (now - _phaseEnteredMs) / 60000UL;
        uint8_t span = (cfg.hardVol > cfg.gentleStartVol)
                     ? (cfg.hardVol - cfg.gentleStartVol) : 0;
        uint32_t ramp = cfg.gentleRampMin ? cfg.gentleRampMin : 1;
        uint32_t add = (elapsedMin >= ramp) ? span : (span * elapsedMin) / ramp;
        _volume = cfg.gentleStartVol + (uint8_t)add;
        tone = GENTLE_TONE_HZ;
        period = GENTLE_PERIOD_MS;
        onMs = GENTLE_BEEP_MS;
    } else {
        _volume = cfg.hardVol;
        tone = HARD_TONE_HZ;
        period = HARD_PERIOD_MS;
        onMs = HARD_BEEP_MS;
    }

    uint32_t inCycle = (now - _phaseEnteredMs) % period;
    bool shouldBeOn = inCycle < onMs;

    if (shouldBeOn && !_beepOn) {
        buzzer.playTone(tone, _volume);
        _beepOn = true;
    } else if (!shouldBeOn && _beepOn) {
        buzzer.stop();
        _beepOn = false;
    } else if (shouldBeOn && _beepOn) {
        buzzer.playTone(tone, _volume);
    }
}

void AlarmEngine::dismiss() {
    if (_testMode) { stopTest(); return; }
    if (_phase == ALARM_GENTLE) recordOutcome(OUTCOME_STIRRED_WOKE);
    else if (_phase == ALARM_HARD)
        recordOutcome(_failsafe ? OUTCOME_FAILSAFE : OUTCOME_HARD_WOKE);
    enter(ALARM_DONE);
}

void AlarmEngine::snooze() {
    if (_testMode) { stopTest(); return; }
    if (_phase != ALARM_GENTLE && _phase != ALARM_HARD) return;
    struct tm t;
    if (!net.localTime(&t)) return;
    uint16_t nowMin = t.tm_hour * 60 + t.tm_min;
    _snoozeUntilMin = nowMin + settings.get().snoozeMin;
    enter(ALARM_SNOOZED);
}

void AlarmEngine::testGentle() { _testMode = true; enter(ALARM_GENTLE); }
void AlarmEngine::testHard()   { _testMode = true; enter(ALARM_HARD); }
void AlarmEngine::stopTest()   { _testMode = false; enter(ALARM_ARMED); }

void AlarmEngine::recordOutcome(NightOutcome o) {
    _outcome = o;

    struct tm t;
    NightStats st = sleepModel.computeNightStats();

    _lastNight.outcome = (uint8_t)o;
    _lastNight.epochStamp = (uint32_t)time(NULL);
    if (net.localTime(&t)) {
        _lastNight.wokeHour = t.tm_hour;
        _lastNight.wokeMin = t.tm_min;
    }
    if (st.valid) {
        _lastNight.sleepEfficiencyPct = st.sleepEfficiencyPct;
        _lastNight.totalSleepMin = st.totalSleepMin;
        _lastNight.wasoMin = st.wakeAfterSleepOnsetMin;
        _lastNight.solMin = st.sleepOnsetLatencyMin;
        struct tm t2;
        if (net.localTime(&t2))
            sleepDebt.recordNight((uint16_t)t2.tm_yday, st.totalSleepMin,
                                  st.sleepEfficiencyPct);
    }
    saveLastNight();
    Serial.printf("[alarm] outcome: %s\n", outcomeName());
}

void AlarmEngine::saveLastNight() {
    aprefs.begin(NVS_NAMESPACE, false);
    aprefs.putBytes("night", &_lastNight, sizeof(_lastNight));
    aprefs.end();
}

void AlarmEngine::loadLastNight() {
    aprefs.begin(NVS_NAMESPACE, true);
    if (aprefs.isKey("night")) {
        NightRecord tmp;
        if (aprefs.getBytes("night", &tmp, sizeof(tmp)) == sizeof(tmp))
            _lastNight = tmp;
    }
    aprefs.end();
}

const char* AlarmEngine::phaseName() const {
    switch (_phase) {
        case ALARM_IDLE:       return "idle";
        case ALARM_ARMED:      return "armed";
        case ALARM_SUPPRESSED: return "suppressed";
        case ALARM_GENTLE:     return "gentle";
        case ALARM_HARD:       return "hard";
        case ALARM_SNOOZED:    return "snoozed";
        case ALARM_DONE:       return "done";
    }
    return "?";
}

const char* AlarmEngine::outcomeNameOf(uint8_t o) {
    switch (o) {
        case OUTCOME_SUPPRESSED:   return "skipped - you were already up";
        case OUTCOME_STIRRED_WOKE: return "stirred and woke";
        case OUTCOME_HARD_WOKE:    return "slept to deadline";
        case OUTCOME_FAILSAFE:     return "failsafe fixed-time alarm";
        default:                   return "no data yet";
    }
}

const char* AlarmEngine::outcomeName() const {
    return outcomeNameOf((uint8_t)_outcome);
}
