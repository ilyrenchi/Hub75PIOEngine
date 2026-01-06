#pragma once
#include <Arduino.h>
#include "PanelGeometry.h"
#include "Hub75PIOEngine.h"

class SmartMatrixRP2040 {
public:
    explicit SmartMatrixRP2040(Hub75PIOEngine& engine);

    void begin();
    void setBrightness(uint8_t brightness);

    void fillScreen(uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void show();

    uint16_t width()  const { return _geo.width; }
    uint16_t height() const { return _geo.height; }

    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

private:
    Hub75PIOEngine& _engine;
    PanelGeometry   _geo;
    uint16_t*       _drawBuffer;
};
