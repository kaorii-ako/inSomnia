#include "thermal.h"
#include "settings.h"
#include <Wire.h>

Thermal thermal;

#define FRAME_INTERVAL_MS   250     // 4 Hz is plenty for sleep movement
#define STALE_MS            4000
#define MOTION_DEADBAND_C   0.25f   // below this a pixel delta is sensor noise
#define COUNT_GAIN          400.0f  // motion index -> integer-ish counts

Thermal::Thermal()
    : _present(false), _lastFrameMs(0), _lastGoodMs(0), _havePrev(false),
      _motionIndex(0), _peakC(0), _ambientC(0), _inBed(false), _warmPixels(0),
      _cx(0), _cy(0), _prevCx(0), _prevCy(0), _centroidShift(0),
      _epochAccum(0), _lastEpochCounts(0), _epochFrames(0), _epochInBedFrames(0),
      _rx0(0), _ry0(0), _rx1(TH_W - 1), _ry1(TH_H - 1) {
    memset(_frame, 0, sizeof(_frame));
    memset(_prev, 0, sizeof(_prev));
}

bool Thermal::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);

    if (!_mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
        Serial.println("[thermal] MLX90640 not found");
        _present = false;
        return false;
    }

    _mlx.setMode(MLX90640_CHESS);
    _mlx.setResolution(MLX90640_ADC_18BIT);
    _mlx.setRefreshRate(MLX90640_4_HZ);

    _present = true;
    _lastGoodMs = millis();
    Serial.println("[thermal] MLX90640 online (32x24 @ 4Hz)");
    return true;
}

void Thermal::setRoi(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    if (x1 >= TH_W) x1 = TH_W - 1;
    if (y1 >= TH_H) y1 = TH_H - 1;
    if (x0 > x1) { uint8_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { uint8_t t = y0; y0 = y1; y1 = t; }
    _rx0 = x0; _ry0 = y0; _rx1 = x1; _ry1 = y1;
}

void Thermal::getRoi(uint8_t* x0, uint8_t* y0, uint8_t* x1, uint8_t* y1) const {
    if (x0) *x0 = _rx0;
    if (y0) *y0 = _ry0;
    if (x1) *x1 = _rx1;
    if (y1) *y1 = _ry1;
}

void Thermal::update() {
    if (!_present) return;

    uint32_t now = millis();
    if (now - _lastFrameMs < FRAME_INTERVAL_MS) return;
    _lastFrameMs = now;

    if (_mlx.getFrame(_frame) != 0) return;   // keep the previous frame
    _lastGoodMs = now;
    _epochFrames++;

    const Settings& cfg = settings.get();

    // Ambient estimate: the coolest pixels in the scene are the room, not you.
    // A simple low percentile is far more robust than a mean, because the body
    // and any warm electronics skew a mean badly.
    float sorted_min = 1000.0f, peak = -1000.0f;
    for (int i = 0; i < TH_PIXELS; i++) {
        if (_frame[i] < sorted_min) sorted_min = _frame[i];
        if (_frame[i] > peak) peak = _frame[i];
    }
    _ambientC = sorted_min;
    _peakC = peak;

    // A pixel counts as "body" if it is meaningfully warmer than the room.
    // A duvet attenuates skin temperature, so the margin has to be modest --
    // this is why it is a tunable setting rather than a hard-coded number.
    float bodyThresh = _ambientC + (cfg.bodyMarginTenthC / 10.0f);

    uint32_t warm = 0;
    double sx = 0, sy = 0, sw = 0;
    for (uint8_t y = _ry0; y <= _ry1; y++) {
        for (uint8_t x = _rx0; x <= _rx1; x++) {
            float t = _frame[y * TH_W + x];
            if (t > bodyThresh) {
                warm++;
                float w = t - bodyThresh;
                sx += x * w; sy += y * w; sw += w;
            }
        }
    }
    _warmPixels = (uint16_t)warm;
    _inBed = warm >= cfg.inBedMinPixels;

    _prevCx = _cx; _prevCy = _cy;
    if (sw > 0) { _cx = (float)(sx / sw); _cy = (float)(sy / sw); }
    _centroidShift = (_havePrev && _inBed)
        ? sqrtf((_cx - _prevCx) * (_cx - _prevCx) + (_cy - _prevCy) * (_cy - _prevCy))
        : 0.0f;

    // Motion index: mean absolute frame-to-frame change inside the ROI, with a
    // deadband so sensor noise does not accumulate into fake movement all night.
    if (_havePrev) {
        double sum = 0; uint32_t n = 0;
        for (uint8_t y = _ry0; y <= _ry1; y++) {
            for (uint8_t x = _rx0; x <= _rx1; x++) {
                int i = y * TH_W + x;
                float d = fabsf(_frame[i] - _prev[i]);
                if (d > MOTION_DEADBAND_C) sum += d;
                n++;
            }
        }
        _motionIndex = n ? (float)(sum / n) : 0.0f;
        _epochAccum += _motionIndex * COUNT_GAIN;
    }

    if (_inBed) _epochInBedFrames++;
    memcpy(_prev, _frame, sizeof(_frame));
    _havePrev = true;
}

void Thermal::rollEpoch() {
    _lastEpochCounts = (uint32_t)(_epochAccum > 4294967000.0 ? 4294967000.0 : _epochAccum);
    _epochAccum = 0;
    _epochFrames = 0;
    _epochInBedFrames = 0;
}

uint8_t Thermal::epochInBedPercent() const {
    if (_epochFrames == 0) return _inBed ? 100 : 0;
    return (uint8_t)((_epochInBedFrames * 100UL) / _epochFrames);
}

bool Thermal::healthy() const {
    if (!_present) return false;
    return (millis() - _lastGoodMs) < STALE_MS;
}
