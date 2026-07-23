// Board-specific pins + tuning constants. Pin sets are selected by the
// BOARD_ESP32DEV / BOARD_ESP32S3 build flag from platformio.ini.
//
// Strapping-pin note: GPIO4/5 are safe general IO on both boards. On the S3 we
// avoid GPIO0/3/45/46 (strapping) and GPIO35-37 (octal PSRAM).
#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Pin assignments
// ---------------------------------------------------------------------------
#if defined(BOARD_ESP32S3)
static constexpr int PIN_FAN_MOSFET = 4;  // LR7843 signal (isolated MOSFET module)
static constexpr int PIN_DS18B20    = 5;  // 1-Wire data (+ 4.7k pullup to 3V3)
static constexpr int PIN_I2C_SDA    = 8;  // ADS1115 (OLED is on SPI, see below)
static constexpr int PIN_I2C_SCL    = 9;
// SSD1306 OLED via 4-wire software SPI (provisional S3 pins — untested)
static constexpr int PIN_OLED_CLK   = 12; // D0 (SCLK)
static constexpr int PIN_OLED_MOSI  = 11; // D1 (MOSI)
static constexpr int PIN_OLED_DC    = 13;
static constexpr int PIN_OLED_CS    = 10;
static constexpr int PIN_OLED_RST   = 14;
#else // BOARD_ESP32DEV — plain WROOM-32 (the bench board)
static constexpr int PIN_FAN_MOSFET = 4;
static constexpr int PIN_DS18B20    = 5;
static constexpr int PIN_I2C_SDA    = 21; // ADS1115 (OLED is on SPI, see below)
static constexpr int PIN_I2C_SCL    = 22;
// SSD1306 OLED via 4-wire software SPI (module labeled D0/D1/RES/DC/CS).
// Software (bit-banged) SPI on named pins: updates run at 1 Hz so speed is a
// non-issue, and it avoids hardware-SPI's default SS pin (GPIO5) colliding with
// the 1-Wire sensor. Wire: D0->CLK, D1->MOSI, RES->RST, DC->DC, CS->CS.
static constexpr int PIN_OLED_CLK   = 18; // D0 (SCLK)
static constexpr int PIN_OLED_MOSI  = 23; // D1 (MOSI)
static constexpr int PIN_OLED_DC    = 17;
static constexpr int PIN_OLED_CS    = 16;
static constexpr int PIN_OLED_RST   = 25;
#endif

// ---------------------------------------------------------------------------
// Fan control thresholds (deg C). Deliberately BELOW OEM ~98 C for earlier
// engagement on slow technical riding; the OEM thermoswitch is the backstop.
// ---------------------------------------------------------------------------
#ifdef BENCH_MODE
// Bench bring-up: low thresholds you can trip by pinching the probe or dunking
// it in warm tap water — lets you watch the ON/OFF hysteresis without hot water.
static constexpr float FAN_ON_C  = 30.0f;
static constexpr float FAN_OFF_C = 28.0f;
#else
static constexpr float FAN_ON_C  = 88.0f;
static constexpr float FAN_OFF_C = 80.0f;
#endif

// Fan MOSFET drive polarity. TRUE = signal HIGH energises the fan (assumed for
// the LR7843 module). Opto-isolated boards are frequently ACTIVE-LOW — the
// ENTIRE failsafe (boot-off, defer-to-OEM) depends on this being correct.
// VERIFY on the actual module before trusting it (see docs/DECISIONS.md D3).
// If the board turns out active-low, flip this one constant — nothing else.
static constexpr bool FAN_ACTIVE_HIGH = true;

// ---------------------------------------------------------------------------
// Engine-running voltage thresholds (V) — see docs/DECISIONS.md D2.
// ---------------------------------------------------------------------------
static constexpr float RUN_ON_V  = 13.2f;
static constexpr float RUN_OFF_V = 12.8f;

// ---------------------------------------------------------------------------
// Battery voltage divider feeding the ADS1115. Vbatt = Vadc * VDIV_RATIO.
// Nominal 6:1 — CALIBRATE against a multimeter once wired (see handoff).
// ---------------------------------------------------------------------------
static constexpr float VDIV_RATIO = 6.0f;
static constexpr uint8_t ADS_CHANNEL = 0;  // ADS1115 single-ended input A0

// ---------------------------------------------------------------------------
// DS18B20 sentinel reads — must NOT be treated as real temperatures.
//   -127 C : disconnected / CRC fail (DallasTemperature DEVICE_DISCONNECTED_C)
//    85 C  : power-on-reset register default (looks plausible, sits in ON band)
// ---------------------------------------------------------------------------
static constexpr float DS18B20_DISCONNECTED_C = -127.0f;
static constexpr float DS18B20_POWERON_C      = 85.0f;

// ---------------------------------------------------------------------------
// Display geometry (SSD1306 128x64, driven over SPI — no I2C address)
// ---------------------------------------------------------------------------
static constexpr uint8_t OLED_W = 128, OLED_H = 64;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
static constexpr uint32_t LOOP_INTERVAL_MS = 1000;  // control loop period
static constexpr uint32_t NVS_WRITE_MS     = 60000; // motohours persist cadence
