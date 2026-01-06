#include <Arduino.h>

#include "PanelGeometry.h"
#include "Hub75PIOEngine.h"
#include "SmartMatrixRP2040.h"
#include <cstring>

// ==================================================
// HUB75 PIN 定義（依你目前實測可用的排列）
// ==================================================

static const uint8_t dataPins[6] = {10, 11, 12, 13, 14, 15};
static const uint8_t PIN_R1 = dataPins[0];
static const uint8_t PIN_G1 = dataPins[1];
static const uint8_t PIN_B1 = dataPins[2];
static const uint8_t PIN_R2 = dataPins[3];
static const uint8_t PIN_G2 = dataPins[4];
static const uint8_t PIN_B2 = dataPins[5];

static const uint8_t PIN_CLK = 6;
static const uint8_t PIN_LAT = 7;
static const uint8_t PIN_OE  = 8;
static const uint8_t PIN_A   = 9;
static const uint8_t PIN_B   = 22;
static const uint8_t PIN_C   = 26;
static const int8_t  PIN_D   = -1;   // 1/8 掃描不使用 D

// ==================================================
// 面板參數：P5 64×32，1/8 掃描
// ==================================================

static PanelGeometry panelGeo = {
    .width     = 64,
    .height    = 32,   // 先維持 32，避免高度不一致
    .scanLines = 8
};

// ==================================================
// Engine / Matrix
// ==================================================

Hub75PIOEngine hub75(panelGeo,
                     PIN_R1, PIN_G1, PIN_B1,
                     PIN_R2, PIN_G2, PIN_B2,
                     PIN_A,  PIN_B,  PIN_C, PIN_D,
                     PIN_LAT, PIN_OE, PIN_CLK);

SmartMatrixRP2040 matrix(hub75);

// =======================================================
// 固定 RAW 單點測試：只畫一顆固定點
// =======================================================

static void testSingleDotRaw() {
    uint16_t w = matrix.width();
    uint16_t h = matrix.height();

    uint16_t* fb = hub75.getDrawBuffer();
    std::memset(fb, 0, (size_t)w * (size_t)h * sizeof(uint16_t));

    uint16_t x = 5;
    uint16_t y = 5;
    fb[(size_t)y * (size_t)w + (size_t)x] =
        SmartMatrixRP2040::color565(255, 255, 255);

    hub75.swapBuffers();
}

// =======================================================
// 移動 RAW 單點測試：一顆點在 64x32 上掃描
// =======================================================

// 保留 panelGeo / hub75 / matrix 宣告不動

// =======================================================
// 移動 RAW 單點測試：一顆點在 64x32 上掃描
// =======================================================

static void drawMovingDotRaw(uint16_t frame) {
    uint16_t w = matrix.width();   // 64
    uint16_t h = matrix.height();  // 32

    uint16_t* fb = hub75.getDrawBuffer();
    std::memset(fb, 0, (size_t)w * (size_t)h * sizeof(uint16_t));

    // 做一個簡單的「蛇形路徑」，確定 X/Y 都有在變
    uint16_t x = frame % w;
    uint16_t y = (frame / w) % h;  // 每走完一行再往下一行

    fb[(size_t)y * (size_t)w + (size_t)x] =
        SmartMatrixRP2040::color565(255, 255, 255);

    hub75.swapBuffers();
}

// ==================================================
// setup
// ==================================================

void setup() {
    Serial.begin(115200);

    hub75.begin();
    // hub75.setRefreshMode(Hub75PIOEngine::RefreshMode::CPU_ONLY);
    hub75.setRefreshMode(Hub75PIOEngine::RefreshMode::PIO_BLOCKING);
    hub75.start();

    matrix.begin();
    matrix.setBrightness(150);

    Serial.println("Start: MOVING RAW dot test.");
}

// ==================================================
// loop：持續刷新 + 移動點
// ==================================================

void loop() {
    static uint16_t frame = 0;
    static uint32_t lastMs = 0;
    uint32_t now = millis();

    // 每 60ms 更新一次位置
    if (now - lastMs >= 60) {
        drawMovingDotRaw(frame++);
        lastMs = now;
    }

    // 不停刷新
    hub75.serviceCPU();
}
