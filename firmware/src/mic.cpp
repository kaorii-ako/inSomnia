#include "mic.h"
#include "settings.h"

Mic mic;

#define NOISE_FLOOR_ALPHA  0.005f
#define STALE_MS           3000

Mic::Mic()
    : _present(false), _lastGoodMs(0), _rms(0), _peak(0), _dbfs(-96.0f),
      _noiseFloor(200.0f), _blocks(0), _loudBlocks(0), _lastLoudFraction(0) {}

bool Mic::begin() {
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = MIC_SAMPLE_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = MIC_BLOCK_SAMPLES;
    cfg.use_apll = false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCLK;
    pins.ws_io_num = I2S_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = I2S_DIN;

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
        Serial.println("[mic] i2s_driver_install failed");
        _present = false;
        return false;
    }
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        Serial.println("[mic] i2s_set_pin failed");
        i2s_driver_uninstall(I2S_NUM_0);
        _present = false;
        return false;
    }

    _present = true;
    _lastGoodMs = millis();
    Serial.println("[mic] INMP441 I2S online");
    return true;
}

void Mic::update() {
    if (!_present) return;

    size_t bytesRead = 0;
    if (i2s_read(I2S_NUM_0, _buf, sizeof(_buf), &bytesRead, 0) != ESP_OK) return;

    size_t n = bytesRead / sizeof(int32_t);
    if (n == 0) return;

    // INMP441 delivers 24-bit data left-aligned in a 32-bit slot.
    // >> 14 brings it into a comfortable int16-ish range.
    double sumSq = 0;
    int32_t peak = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t s = _buf[i] >> 14;
        sumSq += (double)s * (double)s;
        int32_t a = s < 0 ? -s : s;
        if (a > peak) peak = a;
    }

    double r = sqrt(sumSq / (double)n);
    _rms = (uint16_t)(r > 65535.0 ? 65535.0 : r);
    _peak = (uint16_t)(peak > 65535 ? 65535 : peak);
    _dbfs = (r > 1.0) ? 20.0f * log10f((float)r / 32768.0f) : -96.0f;

    _noiseFloor += NOISE_FLOOR_ALPHA * ((float)_rms - _noiseFloor);
    if (_noiseFloor < 1.0f) _noiseFloor = 1.0f;

    _blocks++;
    if (_rms > settings.get().micStirringRms) _loudBlocks++;

    _lastGoodMs = millis();
}

void Mic::rollEpoch() {
    _lastLoudFraction = loudFraction();
    _blocks = 0;
    _loudBlocks = 0;
}

bool Mic::healthy() const {
    if (!_present) return false;
    return (millis() - _lastGoodMs) < STALE_MS;
}
