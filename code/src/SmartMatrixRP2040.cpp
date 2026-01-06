#include "SmartMatrixRP2040.h"

SmartMatrixRP2040::SmartMatrixRP2040(Hub75PIOEngine& engine)
    : _engine(engine),
      _geo(engine.geometry()),
      _drawBuffer(nullptr)
{
}

void SmartMatrixRP2040::begin() {
    // 由外部決定 _engine.setRefreshMode()，這裡只做 begin/start
    _engine.begin();
    _engine.start();
    _drawBuffer = _engine.getDrawBuffer();
}

void SmartMatrixRP2040::setBrightness(uint8_t brightness) {
    _engine.setBrightness(brightness);
}

void SmartMatrixRP2040::fillScreen(uint16_t color) {
    if (!_drawBuffer) return;
    size_t pixelCount = static_cast<size_t>(_geo.width) * _geo.height;
    for (size_t i = 0; i < pixelCount; ++i) {
        _drawBuffer[i] = color;
    }
}

void SmartMatrixRP2040::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!_drawBuffer) return;
    if (x < 0 || y < 0 || x >= _geo.width || y >= _geo.height) return;

    // 如需左右/上下翻轉，可在這裡加入 x/y flip
    // x = _geo.width - 1 - x;
    // y = _geo.height - 1 - y;

    size_t index = static_cast<size_t>(y) * _geo.width + x;
    _drawBuffer[index] = color;
}

void SmartMatrixRP2040::show() {
    _engine.swapBuffers();
    _drawBuffer = _engine.getDrawBuffer();
}

uint16_t SmartMatrixRP2040::color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) |
           ((g & 0xFC) << 3) |
           (b >> 3);
}

