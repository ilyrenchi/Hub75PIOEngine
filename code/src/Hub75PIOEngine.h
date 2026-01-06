#pragma once

#include <Arduino.h>
#include "PanelGeometry.h"

#include <hardware/pio.h>
#include "hub75_stream_1bit.pio.h"

// ================================================
// Row Address (A/B/C) 位元對應（用來快速修正 A/B/C 接線順序）
// rowAddr 的 bit0/bit1/bit2 預設對應 A/B/C。
// ================================================
#define HUB75_ROWADDR_BIT_A 0
#define HUB75_ROWADDR_BIT_B 1
#define HUB75_ROWADDR_BIT_C 2

class Hub75PIOEngine {
public:
    enum class RefreshMode {
        CPU_ONLY,
        PIO_BLOCKING, // CPU 準備 row buffer + pio_sm_put_blocking
        PIO_DMA       // 預留：之後做真正 DMA
    };

    Hub75PIOEngine(const PanelGeometry& geo,
                   uint8_t pinR1, uint8_t pinG1, uint8_t pinB1,
                   uint8_t pinR2, uint8_t pinG2, uint8_t pinB2,
                   uint8_t pinA, uint8_t pinB, uint8_t pinC, int8_t pinD,
                   uint8_t pinLAT, uint8_t pinOE, uint8_t pinCLK);

    inline void setRefreshMode(RefreshMode mode) { _mode = mode; }

    void begin();
    void start();
    void stop();
    void setBrightness(uint8_t brightness);

    uint16_t* getDrawBuffer();
    void swapBuffers();

    void serviceCPU();

    const PanelGeometry& geometry() const { return _geo; }

private:
    PanelGeometry _geo;

    // 掃描步進：rowAddr(0..7) × subRow(0..1)
    uint16_t _scanStep = 0;

    // HUB75 pins
    uint8_t _pinR1, _pinG1, _pinB1;
    uint8_t _pinR2, _pinG2, _pinB2;
    uint8_t _pinA, _pinB, _pinC;
    int8_t  _pinD;
    uint8_t _pinLAT, _pinOE, _pinCLK;

    // 雙 buffer
    uint16_t* _frameBuffers[2];
    uint8_t   _frontIndex;
    uint8_t   _backIndex;

    uint8_t _brightness;
    bool    _running;
    RefreshMode _mode;

    void initGPIO_();
    void freeBuffers_();

    inline void rgb565ToRgb888_(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
        uint8_t r5 = (c >> 11) & 0x1F;
        uint8_t g6 = (c >> 5)  & 0x3F;
        uint8_t b5 =  c        & 0x1F;
        r = (r5 * 527 + 23) >> 6;
        g = (g6 * 259 + 33) >> 6;
        b = (b5 * 527 + 23) >> 6;
    }

    // CPU 掃描：目前只輸出 topRow，bottom 關掉
    void scanRowCPU(uint8_t rowAddr, uint8_t subRow, uint32_t onTimeUs);

    // ===== Column mapping hook（CPU 與 PIO 共用） =====
    inline uint16_t mapColFinal_(uint16_t col, uint16_t width) {
        (void)width;
        return col;
    }

    // ===== 像素 on/off 判斷 =====
    static const uint8_t HUB75_LOW_THRESHOLD = 1;

    // ===== PIO-only（先不啟用 DMA） =====
    PIO  _pio = pio0;
    uint _sm  = 0;
    bool _pioReady = false;

    uint16_t  _rowPixels   = 0;        // 面板寬度
    uint32_t* _pioRowBuffer = nullptr; // 每筆 6bit RGB packed in LSB

    void initPio_();
    void preparePioRowBuffer_(uint8_t rowAddr, uint8_t subRow);
    void scanRowPioBlocking_(uint8_t rowAddr, uint8_t subRow, uint32_t onTimeUs);
    void scanRowPioDma_(uint8_t rowAddr, uint8_t subRow, uint32_t onTimeUs);
};
