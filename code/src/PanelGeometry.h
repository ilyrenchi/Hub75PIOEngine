#pragma once
#include <Arduino.h>

struct PanelGeometry {
    uint16_t width;      // 例如 64
    uint16_t height;     // 例如 32
    uint8_t  scanLines;  // 例如 8 表示 1/8 掃描
};
