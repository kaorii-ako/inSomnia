#include "buzzer.h"

Buzzer buzzer;

Buzzer::Buzzer() : _playing(false), _freq(BUZZER_IDLE_FREQ), _volume(128) {}

bool Buzzer::begin() {
    ledcSetup(BUZZER_CHANNEL, BUZZER_IDLE_FREQ, BUZZER_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
    ledcWrite(BUZZER_CHANNEL, 0);
    return true;
}

void Buzzer::playTone(uint16_t frequency, uint8_t volume) {
    if (frequency == 0 || volume == 0) {
        stop();
        return;
    }
    _freq = frequency;
    _volume = volume;
    _playing = true;
    // ledcWriteTone forces 50% duty, so the volume write must come after it.
    ledcWriteTone(BUZZER_CHANNEL, frequency);
    ledcWrite(BUZZER_CHANNEL, volume);
}

void Buzzer::stop() {
    ledcWrite(BUZZER_CHANNEL, 0);
    _playing = false;
}

void Buzzer::beepBlocking(uint16_t frequency, uint8_t volume, uint16_t durationMs) {
    playTone(frequency, volume);
    delay(durationMs);
    stop();
}
