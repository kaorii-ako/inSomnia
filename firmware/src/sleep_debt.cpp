#include "sleep_debt.h"
#include "settings.h"
#include "config.h"
#include <Preferences.h>

SleepDebt sleepDebt;
static Preferences dprefs;

SleepDebt::SleepDebt() : _head(0), _count(0) {
    memset(_n, 0, sizeof(_n));
}

void SleepDebt::begin() { load(); }

void SleepDebt::load() {
    dprefs.begin(NVS_NAMESPACE, true);
    if (dprefs.isKey("debt")) {
        struct { NightSleep n[DEBT_NIGHTS]; uint8_t head, count; } blob;
        if (dprefs.getBytes("debt", &blob, sizeof(blob)) == sizeof(blob)) {
            memcpy(_n, blob.n, sizeof(_n));
            _head = blob.head % DEBT_NIGHTS;
            _count = blob.count > DEBT_NIGHTS ? DEBT_NIGHTS : blob.count;
        }
    }
    dprefs.end();
}

void SleepDebt::save() {
    struct { NightSleep n[DEBT_NIGHTS]; uint8_t head, count; } blob;
    memcpy(blob.n, _n, sizeof(_n));
    blob.head = _head; blob.count = _count;
    dprefs.begin(NVS_NAMESPACE, false);
    dprefs.putBytes("debt", &blob, sizeof(blob));
    dprefs.end();
}

void SleepDebt::recordNight(uint16_t yday, uint16_t totalSleepMin, uint8_t eff) {
    // Don't double-record the same night if the alarm resolves more than once.
    if (_count > 0) {
        uint8_t last = (uint8_t)((_head + DEBT_NIGHTS - 1) % DEBT_NIGHTS);
        if (_n[last].valid && _n[last].yday == yday) {
            _n[last].totalMin = totalSleepMin;
            _n[last].efficiency = eff;
            save();
            return;
        }
    }
    _n[_head].yday = yday;
    _n[_head].totalMin = totalSleepMin;
    _n[_head].efficiency = eff;
    _n[_head].valid = 1;
    _head = (uint8_t)((_head + 1) % DEBT_NIGHTS);
    if (_count < DEBT_NIGHTS) _count++;
    save();
    Serial.printf("[debt] night logged: %u min, cumulative debt %ld min\n",
                  totalSleepMin, (long)cumulativeDebtMin());
}

const NightSleep& SleepDebt::night(uint8_t agoIdx) const {
    uint8_t i = (uint8_t)((_head + DEBT_NIGHTS - 1 - (agoIdx % DEBT_NIGHTS)) % DEBT_NIGHTS);
    return _n[i];
}

uint16_t SleepDebt::nightsRecorded() const { return _count; }

int32_t SleepDebt::cumulativeDebtMin() const {
    int32_t need = settings.get().sleepNeedMin;
    int32_t total = 0;
    for (uint8_t k = 0; k < _count; k++) {
        const NightSleep& n = night(k);
        if (!n.valid) continue;
        int32_t d = need - (int32_t)n.totalMin;
        if (d > 0) total += d;
    }
    return total;
}

int32_t SleepDebt::recentDebtMin() const {
    int32_t need = settings.get().sleepNeedMin;
    double acc = 0, wsum = 0;
    for (uint8_t k = 0; k < _count; k++) {
        const NightSleep& n = night(k);
        if (!n.valid) continue;
        double w = exp(-(double)k / 5.0);      // ~5 night half-life
        int32_t d = need - (int32_t)n.totalMin;
        if (d < 0) d = 0;
        acc += w * d;
        wsum += w;
    }
    if (wsum <= 0) return 0;
    // scale back to a comparable "minutes across the window" figure
    return (int32_t)(acc / wsum * (double)_count);
}

uint16_t SleepDebt::averageSleepMin() const {
    if (_count == 0) return 0;
    uint32_t t = 0; uint8_t v = 0;
    for (uint8_t k = 0; k < _count; k++) {
        const NightSleep& n = night(k);
        if (n.valid) { t += n.totalMin; v++; }
    }
    return v ? (uint16_t)(t / v) : 0;
}

uint8_t SleepDebt::wakeShiftMinutes() const {
    const Settings& cfg = settings.get();
    if (!cfg.debtAwareEnabled || _count < 3) return 0;

    // One full night of deficit (== sleepNeed) maps to the full shift.
    int32_t debt = recentDebtMin();
    if (debt <= 0) return 0;
    int32_t full = cfg.sleepNeedMin;
    if (full <= 0) return 0;
    int32_t shift = (debt * cfg.maxDebtShiftMin) / full;
    if (shift > cfg.maxDebtShiftMin) shift = cfg.maxDebtShiftMin;
    return (uint8_t)shift;
}

void SleepDebt::clear() {
    memset(_n, 0, sizeof(_n));
    _head = 0; _count = 0;
    save();
}
