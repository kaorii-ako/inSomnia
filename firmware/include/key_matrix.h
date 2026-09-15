#pragma once

#include <Arduino.h>
#include "pin_config.h"

class KeyMatrix {
public:
    KeyMatrix();

    bool begin();
    void update();

    uint16_t state() const { return _stable; }
    uint16_t rawState() const { return _raw; }
    bool isPressed(uint8_t keyIndex) const;
    bool wasJustPressed(uint8_t keyIndex);
    bool anyPressed() const { return _stable != 0; }

private:
    uint16_t shiftIn16();

    uint16_t _raw;
    uint16_t _candidate;
    uint16_t _stable;
    uint16_t _previous;
    uint16_t _edges;
    uint32_t _lastChangeMs;
};

extern KeyMatrix keys;
