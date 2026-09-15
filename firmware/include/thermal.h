#pragma once

#include <Arduino.h>
#include <Adafruit_MLX90640.h>
#include "pin_config.h"

// MLX90640 32x24 far-infrared array, on the nightstand, aimed at the bed.
//
// This replaces BOTH the mattress accelerometer and the PIR. Three things
// come out of it:
//
//   1. motionIndex()  — mean absolute frame-to-frame temperature change.
//      This is the contactless equivalent of an actigraphy count, and it is
//      what Cole-Kripke consumes. Video actigraphy has been validated against
//      polysomnography at Cohen's kappa 0.733, comparable to wrist actigraphy.
//
//   2. inBed()        — a warm, person-sized region inside the bed ROI.
//      Far stronger than a PIR for "are you actually in bed", because it can
//      tell a body in the bed from a warm object elsewhere in the room.
//
//   3. centroid drift — where the warm mass sits, so large position shifts
//      are separable from small twitches.
//
// What this does NOT do: classify sleeping posture. A duvet attenuates the
// thermal signature, and the published work that classifies posture under
// blankets used an infrared DEPTH camera with synthetic blanket augmentation.
// At 32x24 through a duvet that is a research problem, so this reports
// movement and position, not posture.

#define TH_W 32
#define TH_H 24
#define TH_PIXELS (TH_W * TH_H)

class Thermal {
public:
    Thermal();

    bool begin();
    void update();

    bool present() const { return _present; }
    bool healthy() const;

    float motionIndex() const { return _motionIndex; }
    float peakTempC() const { return _peakC; }
    float ambientTempC() const { return _ambientC; }
    bool inBed() const { return _inBed; }
    uint16_t warmPixels() const { return _warmPixels; }
    float centroidX() const { return _cx; }
    float centroidY() const { return _cy; }
    float centroidShift() const { return _centroidShift; }

    // epoch accumulation, mirrors the old accelerometer counts API
    uint32_t epochCounts() const { return (uint32_t)_epochAccum; }
    uint32_t lastEpochCounts() const { return _lastEpochCounts; }
    uint8_t  epochInBedPercent() const;
    void rollEpoch();

    const float* frame() const { return _frame; }

    // Bed region of interest, in pixel coords. Defaults to the whole frame.
    void setRoi(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
    void getRoi(uint8_t* x0, uint8_t* y0, uint8_t* x1, uint8_t* y1) const;

private:
    Adafruit_MLX90640 _mlx;
    bool _present;
    uint32_t _lastFrameMs;
    uint32_t _lastGoodMs;

    float _frame[TH_PIXELS];
    float _prev[TH_PIXELS];
    bool _havePrev;

    float _motionIndex;
    float _peakC;
    float _ambientC;
    bool _inBed;
    uint16_t _warmPixels;
    float _cx, _cy, _prevCx, _prevCy, _centroidShift;

    double _epochAccum;
    uint32_t _lastEpochCounts;
    uint32_t _epochFrames;
    uint32_t _epochInBedFrames;

    uint8_t _rx0, _ry0, _rx1, _ry1;
};

extern Thermal thermal;
