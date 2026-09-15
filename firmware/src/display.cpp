#include "display.h"

Display display;

Display::Display()
    : _tft(&SPI, TFT_CS, TFT_DC, TFT_RST), _w(0), _h(0), _brightness(255) {}

bool Display::begin() {
    // ESP32-S3 routes SPI through the GPIO matrix, so the bus is remapped
    // onto our pins rather than using the board defaults. MISO is unused.
    SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);

    ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION);
    ledcAttachPin(TFT_BL, BACKLIGHT_CHANNEL);
    ledcWrite(BACKLIGHT_CHANNEL, 0);

    // INITR_BLACKTAB suits most 1.8" 128x160 modules. If the image is shifted
    // or the colours are inverted, try INITR_GREENTAB or INITR_REDTAB.
    _tft.initR(INITR_BLACKTAB);
    _tft.setRotation(TFT_ROTATION);

    _w = _tft.width();
    _h = _tft.height();

    _tft.fillScreen(ST77XX_BLACK);
    setBrightness(255);
    return true;
}

void Display::clear(uint16_t color) {
    _tft.fillScreen(color);
}

void Display::drawText(const char* text, int16_t x, int16_t y,
                       uint16_t color, uint8_t size) {
    _tft.setTextSize(size);
    _tft.setTextColor(color);
    _tft.setCursor(x, y);
    _tft.print(text);
}

void Display::drawTextCentered(const char* text, int16_t y,
                               uint16_t color, uint8_t size) {
    int16_t textW = (int16_t)strlen(text) * 6 * size;
    int16_t x = (_w - textW) / 2;
    if (x < 0) x = 0;
    drawText(text, x, y, color, size);
}

void Display::fillRow(int16_t y, int16_t h, uint16_t color) {
    _tft.fillRect(0, y, _w, h, color);
}

void Display::setBacklight(bool on) {
    ledcWrite(BACKLIGHT_CHANNEL, on ? _brightness : 0);
}

void Display::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    ledcWrite(BACKLIGHT_CHANNEL, brightness);
}
