#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "pin_config.h"

class Display {
public:
    Display();

    bool begin();
    void clear(uint16_t color = ST77XX_BLACK);
    void drawText(const char* text, int16_t x, int16_t y,
                  uint16_t color = ST77XX_WHITE, uint8_t size = 1);
    void drawTextCentered(const char* text, int16_t y,
                          uint16_t color = ST77XX_WHITE, uint8_t size = 1);
    void fillRow(int16_t y, int16_t h, uint16_t color = ST77XX_BLACK);

    void setBacklight(bool on);
    void setBrightness(uint8_t brightness);
    uint8_t brightness() const { return _brightness; }

    int16_t width()  const { return _w; }
    int16_t height() const { return _h; }

    Adafruit_ST7735& tft() { return _tft; }

private:
    Adafruit_ST7735 _tft;
    int16_t _w, _h;
    uint8_t _brightness;
};

extern Display display;
