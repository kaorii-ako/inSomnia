#pragma once

#include <Arduino.h>
#include "pin_config.h"

class Buzzer {
public:
    Buzzer();

    bool begin();
    void playTone(uint16_t frequency, uint8_t volume);
    void stop();
    void beepBlocking(uint16_t frequency, uint8_t volume, uint16_t durationMs);
    bool isPlaying() const { return _playing; }

private:
    bool _playing;
    uint16_t _freq;
    uint8_t _volume;
};

extern Buzzer buzzer;
