#include "sleep_model.h"
#include "settings.h"

SleepModel sleepModel;

// Cole-Kripke (1992), 1-minute epochs:
//   D = 0.001 * (106*A-4 + 54*A-3 + 58*A-2 + 76*A-1 + 230*A0 + 74*A+1 + 67*A+2)
//   sleep when D < 1
static const float CK_W[7]  = {106.0f, 54.0f, 58.0f, 76.0f, 230.0f, 74.0f, 67.0f};
static const float CK_P     = 0.001f;
static const int   CK_LEAD  = 2;   // epochs of lookahead required
static const int   CK_LAG   = 4;   // epochs of history required

SleepModel::SleepModel()
    : _head(0), _count(0), _state(STATE_UNKNOWN), _stateEnteredMs(0),
      _aboveAwake(0), _belowAwake(0), _aboveStirring(0), _belowStirring(0),
      _outOfBedEpochs(0), _lastCounts(0), _degraded(false) {
    memset(_ring, 0, sizeof(_ring));
}

void SleepModel::begin() {
    resetNight();
}

void SleepModel::resetNight() {
    memset(_ring, 0, sizeof(_ring));
    _head = 0;
    _count = 0;
    _state = STATE_UNKNOWN;
    _stateEnteredMs = millis();
    _aboveAwake = _belowAwake = _aboveStirring = _belowStirring = 0;
    _outOfBedEpochs = 0;
    _lastCounts = 0;
}

uint16_t SleepModel::countsAt(uint16_t epochsAgo) const {
    if (epochsAgo >= _count) return 0;
    int idx = (int)_head - 1 - (int)epochsAgo;
    while (idx < 0) idx += EPOCH_RING;
    return _ring[idx % EPOCH_RING];
}

void SleepModel::pushEpoch(uint16_t counts, float micLoudFraction, uint8_t inBedPercent) {
    _ring[_head] = counts;
    _head = (_head + 1) % EPOCH_RING;
    if (_count < EPOCH_RING) _count++;
    _lastCounts = counts;

    // Out of bed: no warm body-sized mass in the bed region. With a thermal
    // array this is close to a direct measurement rather than an inference,
    // which is exactly why alarm suppression is hung off it and not off the
    // sleep classifier (whose specificity for wake is only 0.35-0.64).
    if (inBedPercent < 20) {
        if (_outOfBedEpochs < 255) _outOfBedEpochs++;
    } else {
        _outOfBedEpochs = 0;
    }

    classify(counts, micLoudFraction, inBedPercent);
}

void SleepModel::classify(uint16_t counts, float micLoudFraction, uint8_t inBedPercent) {
    const Settings& cfg = settings.get();

    // The mic nudges the effective activity level: sustained noise above the
    // floor is weak evidence of stirring. It is a hint, not a decision.
    uint16_t effective = counts;
    if (micLoudFraction > 0.25f) {
        effective += (uint16_t)(cfg.stirringCounts * micLoudFraction);
    }

    uint16_t hiOn  = cfg.awakeCounts;
    uint16_t hiOff = (uint16_t)(cfg.awakeCounts * 0.6f);      // hysteresis band
    uint16_t loOn  = cfg.stirringCounts;
    uint16_t loOff = (uint16_t)(cfg.stirringCounts * 0.6f);

    if (effective > hiOn)  { _aboveAwake++;    _belowAwake = 0; }
    else if (effective < hiOff) { _belowAwake++; _aboveAwake = 0; }

    if (effective > loOn)  { _aboveStirring++; _belowStirring = 0; }
    else if (effective < loOff) { _belowStirring++; _aboveStirring = 0; }

    SleepState next = _state;

    switch (_state) {
        case STATE_UNKNOWN:
            next = (effective > loOn) ? STATE_STIRRING : STATE_ASLEEP;
            break;

        case STATE_ASLEEP:
            if (_aboveAwake >= cfg.awakeConfirmEpochs) next = STATE_AWAKE;
            else if (_aboveStirring >= 1)              next = STATE_STIRRING;
            break;

        case STATE_STIRRING:
            if (_aboveAwake >= cfg.awakeConfirmEpochs) next = STATE_AWAKE;
            else if (_belowStirring >= 2)              next = STATE_ASLEEP;
            break;

        case STATE_AWAKE:
            if (_belowAwake >= 3) next = STATE_STIRRING;
            break;
    }

    // Out of the bed for a sustained stretch means awake, regardless of what
    // the motion thresholds say. You cannot be asleep in a bed you are not in.
    if (_outOfBedEpochs >= OUT_OF_BED_CONFIRM) {
        next = STATE_AWAKE;
    }

    if (next != _state) {
        _state = next;
        _stateEnteredMs = millis();
        _aboveAwake = _belowAwake = _aboveStirring = _belowStirring = 0;
    }
}

uint32_t SleepModel::secondsInState() const {
    return (millis() - _stateEnteredMs) / 1000;
}

const char* SleepModel::stateName() const {
    switch (_state) {
        case STATE_ASLEEP:   return "asleep";
        case STATE_STIRRING: return "stirring";
        case STATE_AWAKE:    return "awake";
        default:             return "unknown";
    }
}

// idx counts epochs back from the newest. Needs CK_LEAD newer epochs to exist,
// so the caller must never ask about the most recent CK_LEAD epochs.
bool SleepModel::coleKripkeSleepAt(uint16_t idx) const {
    if (idx < (uint16_t)CK_LEAD) return false;
    if (idx + CK_LAG >= _count)  return false;

    float scale = (float)settings.get().countScale;
    if (scale < 1.0f) scale = 1.0f;

    float d = 0.0f;
    // weights run A-4 .. A+2; A-4 is the OLDEST, so it sits further back.
    for (int w = 0; w < 7; w++) {
        int offsetFromCentre = (w - 4) * -1;      // w=0 -> +4 back, w=6 -> -2 back
        int ago = (int)idx + offsetFromCentre;
        if (ago < 0 || ago >= (int)_count) continue;
        d += CK_W[w] * ((float)countsAt((uint16_t)ago) / scale);
    }
    return (CK_P * d) < 1.0f;
}

NightStats SleepModel::computeNightStats() const {
    NightStats s = {};
    if (_count < (uint16_t)(CK_LAG + CK_LEAD + 2)) {
        s.valid = false;
        return s;
    }

    s.epochsInBed = _count;

    int firstSleep = -1;
    uint16_t asleep = 0, waso = 0, stirs = 0;
    bool prevSleep = false;

    // walk oldest -> newest, skipping the trailing epochs Cole-Kripke cannot score
    for (int ago = (int)_count - 1 - CK_LAG; ago >= CK_LEAD; ago--) {
        bool sl = coleKripkeSleepAt((uint16_t)ago);
        if (sl) {
            asleep++;
            if (firstSleep < 0) firstSleep = (int)_count - 1 - ago;
        } else if (firstSleep >= 0) {
            waso++;
        }
        if (prevSleep && !sl) stirs++;
        prevSleep = sl;
    }

    s.epochsAsleep = asleep;
    s.totalSleepMin = asleep;                       // 1 epoch = 1 minute
    s.sleepOnsetLatencyMin = (firstSleep < 0) ? 0 : (uint16_t)firstSleep;
    s.wakeAfterSleepOnsetMin = waso;
    s.stirringEpisodes = stirs;
    s.sleepEfficiencyPct = s.epochsInBed
        ? (uint8_t)((asleep * 100UL) / s.epochsInBed) : 0;
    s.valid = true;
    return s;
}
