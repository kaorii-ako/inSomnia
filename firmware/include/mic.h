#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include "pin_config.h"

// INMP441 I2S MEMS microphone.
//
// Phase 3 scope: a calibrated level detector (RMS / peak / dBFS) plus a rolling
// "loud fraction" used as a stirring hint. It is NOT a sound classifier yet --
// the TFLite Micro snore/movement model is future work, and nothing here should
// be described as recognising what a sound is.

#define MIC_SAMPLE_RATE   16000
#define MIC_BLOCK_SAMPLES 512

class Mic {
public:
    Mic();

    bool begin();
    void update();

    bool present() const { return _present; }
    bool healthy() const;

    uint16_t rms() const { return _rms; }
    uint16_t peak() const { return _peak; }
    float dbfs() const { return _dbfs; }
    float noiseFloor() const { return _noiseFloor; }
    uint32_t loudSamplesThisEpoch() const { return _loudBlocks; }
    uint32_t blocksThisEpoch() const { return _blocks; }
    float loudFraction() const {
        return _blocks ? (float)_loudBlocks / (float)_blocks : 0.0f;
    }
    void rollEpoch();
    float lastEpochLoudFraction() const { return _lastLoudFraction; }

private:
    bool _present;
    uint32_t _lastGoodMs;
    int32_t _buf[MIC_BLOCK_SAMPLES];
    uint16_t _rms;
    uint16_t _peak;
    float _dbfs;
    float _noiseFloor;
    uint32_t _blocks;
    uint32_t _loudBlocks;
    float _lastLoudFraction;
};

extern Mic mic;
