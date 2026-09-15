#include "key_matrix.h"

KeyMatrix keys;

KeyMatrix::KeyMatrix()
    : _raw(0), _candidate(0), _stable(0), _previous(0), _edges(0),
      _lastChangeMs(0) {}

bool KeyMatrix::begin() {
    pinMode(SHIFT_CLK, OUTPUT);
    pinMode(SHIFT_LOAD, OUTPUT);
    pinMode(SHIFT_DATA, INPUT);

    digitalWrite(SHIFT_CLK, LOW);
    digitalWrite(SHIFT_LOAD, HIGH);

    _lastChangeMs = millis();
    return true;
}

uint16_t KeyMatrix::shiftIn16() {
    digitalWrite(SHIFT_LOAD, LOW);
    delayMicroseconds(5);
    digitalWrite(SHIFT_LOAD, HIGH);
    delayMicroseconds(5);

    uint16_t bits = 0;
    // QH presents the first bit straight after load, before any clock edge.
    for (int i = 0; i < NUM_KEY_BITS; i++) {
        if (digitalRead(SHIFT_DATA)) {
            bits |= (uint16_t)1 << i;
        }
        digitalWrite(SHIFT_CLK, HIGH);
        delayMicroseconds(5);
        digitalWrite(SHIFT_CLK, LOW);
        delayMicroseconds(5);
    }
    return bits;
}

void KeyMatrix::update() {
    uint16_t bits = shiftIn16();
#if KEY_ACTIVE_LOW
    bits = ~bits;
#endif
    bits &= ((uint16_t)1 << NUM_KEYS) - 1;
    _raw = bits;

    uint32_t now = millis();
    if (bits != _candidate) {
        _candidate = bits;
        _lastChangeMs = now;
        return;
    }

    if (bits != _stable && (now - _lastChangeMs) >= KEY_DEBOUNCE_MS) {
        _previous = _stable;
        _stable = bits;
        _edges |= (uint16_t)(_stable & ~_previous);
    }
}

bool KeyMatrix::isPressed(uint8_t keyIndex) const {
    if (keyIndex >= NUM_KEYS) return false;
    return (_stable >> keyIndex) & 1;
}

bool KeyMatrix::wasJustPressed(uint8_t keyIndex) {
    if (keyIndex >= NUM_KEYS) return false;
    uint16_t mask = (uint16_t)1 << keyIndex;
    if (_edges & mask) {
        _edges &= (uint16_t)~mask;
        return true;
    }
    return false;
}
