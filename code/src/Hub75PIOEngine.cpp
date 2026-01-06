#include "Hub75PIOEngine.h"

Hub75PIOEngine::Hub75PIOEngine(const PanelGeometry& geo,
                               uint8_t pinR1, uint8_t pinG1, uint8_t pinB1,
                               uint8_t pinR2, uint8_t pinG2, uint8_t pinB2,
                               uint8_t pinA, uint8_t pinB, uint8_t pinC, int8_t pinD,
                               uint8_t pinLAT, uint8_t pinOE, uint8_t pinCLK)
: _geo(geo),
  _pinR1(pinR1), _pinG1(pinG1), _pinB1(pinB1),
  _pinR2(pinR2), _pinG2(pinG2), _pinB2(pinB2),
  _pinA(pinA), _pinB(pinB), _pinC(pinC), _pinD(pinD),
  _pinLAT(pinLAT), _pinOE(pinOE), _pinCLK(pinCLK),
  _frontIndex(0), _backIndex(1),
  _brightness(255), _running(false),
  _mode(RefreshMode::CPU_ONLY)
{
    _frameBuffers[0] = nullptr;
    _frameBuffers[1] = nullptr;
}

void Hub75PIOEngine::begin() {
    initGPIO_();

    // 1) frame buffers
    size_t pixelCount = static_cast<size_t>(_geo.width) * _geo.height;
    _frameBuffers[0] = new uint16_t[pixelCount];
    _frameBuffers[1] = new uint16_t[pixelCount];
    memset(_frameBuffers[0], 0, pixelCount * sizeof(uint16_t));
    memset(_frameBuffers[1], 0, pixelCount * sizeof(uint16_t));

    // 2) PIO row buffer
    _rowPixels = _geo.width;
    if (_pioRowBuffer == nullptr && _rowPixels > 0) {
        _pioRowBuffer = new uint32_t[_rowPixels];
        memset(_pioRowBuffer, 0, _rowPixels * sizeof(uint32_t));
    }

    // 3) PIO init（先準備好，之後可切 PIO_BLOCKING）
    if (_mode == RefreshMode::PIO_BLOCKING || _mode == RefreshMode::PIO_DMA) {
        initPio_();
    }
}

void Hub75PIOEngine::start() {
    _running = true;
}

void Hub75PIOEngine::stop() {
    _running = false;
}

void Hub75PIOEngine::setBrightness(uint8_t brightness) {
    _brightness = brightness;
}

uint16_t* Hub75PIOEngine::getDrawBuffer() {
    return _frameBuffers[_backIndex];
}

void Hub75PIOEngine::swapBuffers() {
    uint8_t tmp = _frontIndex;
    _frontIndex = _backIndex;
    _backIndex = tmp;
}

// ======================================================
// 1/8 HUB75 掃描：rowAddr(0..7) × subRow(0..1)
// ======================================================
void Hub75PIOEngine::serviceCPU() {
    if (!_running) return;
    if (!_frameBuffers[0] || !_frameBuffers[1]) return;

    const uint16_t height = _geo.height;      // 32
    const uint8_t  scan   = _geo.scanLines;   // 8
    const uint16_t halfRows       = height / 2;        // 16
    const uint8_t  subRowsPerAddr = halfRows / scan;   // 2
    const uint16_t totalSteps     = scan * subRowsPerAddr; // 16

    uint16_t baseOnTimeUs = 200;
    uint32_t onTimeUs = (static_cast<uint32_t>(baseOnTimeUs) * _brightness) / 255;
    if (onTimeUs == 0) onTimeUs = 1;

    uint16_t step    = _scanStep;       // 0..15
    uint8_t  rowAddr = step % scan;     // 0..7
    uint8_t  subRow  = step / scan;     // 0..1

    // A/B/C(/D) 位址線（與 main 的 wiring 對應）
    digitalWrite(_pinA, (rowAddr & 0x01) ? HIGH : LOW);
    digitalWrite(_pinB, (rowAddr & 0x02) ? HIGH : LOW);
    digitalWrite(_pinC, (rowAddr & 0x04) ? HIGH : LOW);
    if (_pinD >= 0) digitalWrite(_pinD, (rowAddr & 0x08) ? HIGH : LOW);

    // 關燈準備 shift
    digitalWrite(_pinOE, HIGH);

    // 先固定用 CPU 掃描，避免 PIO 設定造成黑屏
    scanRowCPU(rowAddr, subRow, onTimeUs);

    _scanStep++;
    if (_scanStep >= totalSteps) _scanStep = 0;
}

// ======================================================
// CPU 掃描：一次只輸出 topRow（R1/G1/B1），bottom 關掉
// ======================================================
void Hub75PIOEngine::scanRowCPU(uint8_t rowAddr, uint8_t subRow, uint32_t onTimeUs) {
    uint16_t* fb = _frameBuffers[_frontIndex];
    const uint16_t width  = _geo.width;
    const uint16_t height = _geo.height;
    const uint8_t  scan   = _geo.scanLines; // e.g. 8

    const uint16_t halfRows       = height / 2;      // 16
    const uint8_t  subRowsPerAddr = halfRows / scan; // 2
    if (subRow >= subRowsPerAddr) subRow = 0;

    // rowAddr: 0..7, subRow: 0..1 → topRow: 0..15
    uint16_t topRow = (uint16_t)rowAddr * subRowsPerAddr + subRow;
    // 不再做 topRow>=halfRows 的重設，避免整個畫面都被打到 0 行

    // ===== 輸出一整列（只掃 topRow） =====
    for (uint16_t col = 0; col < width; ++col) {
        const uint16_t srcCol = mapColFinal_(col, width);
        uint16_t idxTop = topRow * width + srcCol;

        bool rT = false, gT = false, bT = false;

#if HUB75_LOW_THRESHOLD
        uint16_t cT = fb[idxTop];
        rT = (((cT >> 11) & 0x1F) >= 1);
        gT = (((cT >> 5)  & 0x3F) >= 1);
        bT = (((cT      ) & 0x1F) >= 1);
#else
        uint8_t rT8 = 0, gT8 = 0, bT8 = 0;
        rgb565ToRgb888_(fb[idxTop], rT8, gT8, bT8);
        rT = (rT8 > 127);
        gT = (gT8 > 127);
        bT = (bT8 > 127);
#endif

        // DATA：只有 top 有資料，bottom 全關
        digitalWrite(_pinR1, rT);
        digitalWrite(_pinG1, gT);
        digitalWrite(_pinB1, bT);
        digitalWrite(_pinR2, LOW);
        digitalWrite(_pinG2, LOW);
        digitalWrite(_pinB2, LOW);

        // CLK
        digitalWrite(_pinCLK, LOW);
        digitalWrite(_pinCLK, HIGH);
    }

    // LATCH + OE
    digitalWrite(_pinLAT, HIGH);
    digitalWrite(_pinLAT, LOW);
    digitalWrite(_pinOE, LOW);
    delayMicroseconds(onTimeUs);
    digitalWrite(_pinOE, HIGH);
}

// ======================================================
// PIO 初始化（不含 DMA）
// ======================================================
void Hub75PIOEngine::initPio_() {
    if (_pioReady) return;

    uint offset = pio_add_program(_pio, &hub75_stream_program);
    pio_sm_config c = hub75_stream_program_get_default_config(offset);

    // 6 個 OUT pins：R1..B2
    sm_config_set_out_pins(&c, _pinR1, 6);
    // 1 個 side-set pin：CLK
    sm_config_set_sideset_pins(&c, _pinCLK);
    // SHIFT 方向：右移、LSB first
    sm_config_set_out_shift(&c, true, false, 0);
    // PIO 時鐘（之後可調整）
    sm_config_set_clkdiv(&c, 4.0f);

    for (int i = 0; i < 6; ++i) {
        pio_gpio_init(_pio, _pinR1 + i);
    }
    pio_gpio_init(_pio, _pinCLK);
    pio_sm_set_consecutive_pindirs(_pio, _sm, _pinR1, 6, true);
    pio_sm_set_consecutive_pindirs(_pio, _sm, _pinCLK, 1, true);

    pio_sm_init(_pio, _sm, offset, &c);
    pio_sm_set_enabled(_pio, _sm, true);
    _pioReady = true;
}

// ======================================================
// 把一個 rowAddr/subRow 的 top+bottom 打包成 PIO row buffer
// ======================================================
void Hub75PIOEngine::preparePioRowBuffer_(uint8_t rowAddr, uint8_t subRow) {
    uint16_t* fb = _frameBuffers[_frontIndex];
    const uint16_t width  = _geo.width;
    const uint16_t height = _geo.height;
    const uint8_t  scan   = _geo.scanLines;

    const uint16_t halfRows       = height / 2;
    const uint8_t  subRowsPerAddr = halfRows / scan;
    if (subRow >= subRowsPerAddr) subRow = 0;

    // ★與 CPU 版完全一致的 row 計算
    uint16_t topRow = static_cast<uint16_t>(rowAddr) * subRowsPerAddr + subRow;
    uint16_t botRow = (topRow + halfRows) % height;

    for (uint16_t col = 0; col < width; ++col) {
        const uint16_t srcCol = mapColFinal_(col, width);

        uint16_t idxTop = topRow * width + srcCol;
        uint16_t idxBot = botRow * width + srcCol;

        bool rT=false,gT=false,bT=false;
        bool rB=false,gB=false,bB=false;

        if (HUB75_LOW_THRESHOLD) {
            uint16_t cT = fb[idxTop];
            uint16_t cB = fb[idxBot];
            rT = (((cT >> 11) & 0x1F) >= 1);
            gT = (((cT >> 5)  & 0x3F) >= 1);
            bT = (((cT      ) & 0x1F) >= 1);

            rB = (((cB >> 11) & 0x1F) >= 1);
            gB = (((cB >> 5)  & 0x3F) >= 1);
            bB = (((cB      ) & 0x1F) >= 1);
        } else {
            uint8_t rT8=0,gT8=0,bT8=0;
            uint8_t rB8=0,gB8=0,bB8=0;
            rgb565ToRgb888_(fb[idxTop], rT8, gT8, bT8);
            rgb565ToRgb888_(fb[idxBot], rB8, gB8, bB8);

            rT = (rT8 > 127);
            gT = (gT8 > 127);
            bT = (bT8 > 127);

            rB = (rB8 > 127);
            gB = (gB8 > 127);
            bB = (bB8 > 127);
        }

        uint32_t bits = 0;
        // dataPins[0..5] = R1,G1,B1,R2,G2,B2
        bits |= rT ? (1u << 0) : 0;
        bits |= gT ? (1u << 1) : 0;
        bits |= bT ? (1u << 2) : 0;
        bits |= rB ? (1u << 3) : 0;
        bits |= gB ? (1u << 4) : 0;
        bits |= bB ? (1u << 5) : 0;

        _pioRowBuffer[col] = bits;
    }
}

// ======================================================
// PIO 模式：CPU 準備 row buffer，PIO 負責 shift（不用 DMA）
// ======================================================
void Hub75PIOEngine::scanRowPioBlocking_(uint8_t rowAddr, uint8_t subRow, uint32_t onTimeUs) {
    if (!_pioReady) return;
    if (_pioRowBuffer == nullptr) return;

    // 清 FIFO 避免殘影
    pio_sm_clear_fifos(_pio, _sm);

    preparePioRowBuffer_(rowAddr, subRow);

    const uint16_t width = _geo.width;

    for (uint16_t col = 0; col < width; ++col) {
        pio_sm_put_blocking(_pio, _sm, _pioRowBuffer[col]);
    }

    digitalWrite(_pinLAT, HIGH);
    digitalWrite(_pinLAT, LOW);

    digitalWrite(_pinOE, LOW);
    delayMicroseconds(onTimeUs);
    digitalWrite(_pinOE, HIGH);
}

void Hub75PIOEngine::scanRowPioDma_(uint8_t rowAddr, uint8_t subRow, uint32_t onTimeUs) {
    // 本 no-DMA 版本：先用 blocking 代替，讓你把顯示邏輯「對齊」。
    scanRowPioBlocking_(rowAddr, subRow, onTimeUs);
}

void Hub75PIOEngine::initGPIO_() {
    pinMode(_pinR1, OUTPUT);
    pinMode(_pinG1, OUTPUT);
    pinMode(_pinB1, OUTPUT);
    pinMode(_pinR2, OUTPUT);
    pinMode(_pinG2, OUTPUT);
    pinMode(_pinB2, OUTPUT);

    pinMode(_pinA, OUTPUT);
    pinMode(_pinB, OUTPUT);
    pinMode(_pinC, OUTPUT);
    if (_pinD >= 0) {
        pinMode(_pinD, OUTPUT);
    }

    pinMode(_pinLAT, OUTPUT);
    pinMode(_pinOE, OUTPUT);
    pinMode(_pinCLK, OUTPUT);

    digitalWrite(_pinLAT, LOW);
    digitalWrite(_pinOE, HIGH);
    digitalWrite(_pinCLK, LOW);

    digitalWrite(_pinA, LOW);
    digitalWrite(_pinB, LOW);
    digitalWrite(_pinC, LOW);
    if (_pinD >= 0) digitalWrite(_pinD, LOW);
}

void Hub75PIOEngine::freeBuffers_() {
    if (_frameBuffers[0]) {
        delete[] _frameBuffers[0];
        _frameBuffers[0] = nullptr;
    }
    if (_frameBuffers[1]) {
        delete[] _frameBuffers[1];
        _frameBuffers[1] = nullptr;
    }

    if (_pioRowBuffer) {
        delete[] _pioRowBuffer;
        _pioRowBuffer = nullptr;
    }
    _rowPixels = 0;
}

