// Hub75FinalConfig.h
#pragma once

// ================================================
// HUB75 最終調試參數（經測試驗證）
// 調試完成時間：現在
// 面板規格：64x32，1/8掃描
// ================================================

// 映射參數（最終版本）
#define FINAL_BANK_ROTATION   0
#define FINAL_SWAP_HALVES     false
#define FINAL_REVERSE_BANK    true
#define FINAL_FLIP_HORIZONTAL true   // 注意：這是 true
#define FINAL_DATA_SHIFT      -15    // 注意：這是 -15

// 顏色映射




const uint8_t FINAL_COLOR_MAP[6] = {0, 1, 2, 3, 4, 5};

