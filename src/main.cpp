// CRF250L fan controller — hardware integration layer.
//
// Responsibilities (the ONLY place hardware is touched):
//   * read the DS18B20, classify the reading, feed FanControl, drive the MOSFET
//   * read battery voltage via ADS1115, feed EngineHours, persist to NVS
//   * render status to the OLED and the serial console
// All *decisions* live in the pure libs (lib/FanControl, lib/EngineHours); this
// file only moves data between hardware and those libs.
//
// Failsafe boot rule: the fan MOSFET is driven LOW before anything else, and the
// ESP is purely additive — if it hangs or crashes, the OEM thermoswitch still
// runs the fan on its own independent ground path.
//
// Sensors may be ABSENT during early bench bring-up. Each peripheral is probed
// and its presence tracked, so the firmware runs (and reports) with nothing but
// the bare ESP32 attached — a disconnected DS18B20 exercises the fault path.

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#include "config.h"
#include "FanControl.h"
#include "EngineHours.h"

// ---- hardware objects ----
static OneWire oneWire(PIN_DS18B20);
static DallasTemperature ds18b20(&oneWire);
static Adafruit_ADS1115 ads;
// 4-wire software SPI: (w, h, mosi, sclk, dc, rst, cs). Bit-banged on named pins
// to avoid hardware SPI's default SS grabbing GPIO5 (the 1-Wire sensor pin).
static Adafruit_SSD1306 display(OLED_W, OLED_H,
                                PIN_OLED_MOSI, PIN_OLED_CLK,
                                PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);
static Preferences prefs;

// ---- pure controllers (thresholds from config.h) ----
static fan::Controller fanCtl({FAN_ON_C, FAN_OFF_C});
static engine::HourMeter hourMeter({RUN_ON_V, RUN_OFF_V});

// ---- peripheral presence (set in setup) ----
static bool adsOk = false;
static bool oledOk = false;

// ---- loop timing / persistence cadence ----
static uint32_t lastLoopMs = 0;
static uint32_t lastNvsMs = 0;

// Hardware->logic boundary: map a raw DallasTemperature read into a trustworthy
// or untrustworthy TempReading. This is the guard that stops the two DS18B20
// failure reads from being acted on as real temperatures.
//
// Caveat on the 85.0 C case: the power-on default is *exactly* 85.0, and a real
// engine can genuinely reach 85 C — so this can false-positive on a true 85.0
// read. It's largely harmless here (85 < FAN_ON_C, so the fan outcome is the
// same OFF either way in the common case), but a boot-time-only guard is a
// candidate refinement if it ever matters. See handoff / DECISIONS for context.
static fan::TempReading classifyTemp(float raw) {
    if (raw <= DS18B20_DISCONNECTED_C + 1.0f) {
        return {raw, fan::TempStatus::Disconnected};
    }
    if (raw == DS18B20_POWERON_C) {
        return {raw, fan::TempStatus::PowerOnDefault};
    }
    return {raw, fan::TempStatus::Valid};
}

// Drive the fan MOSFET respecting module polarity (config FAN_ACTIVE_HIGH).
// Centralising this means an active-low module is a one-constant fix, and the
// failsafe "off" level is defined in exactly one place.
static inline void fanWrite(bool on) {
    digitalWrite(PIN_FAN_MOSFET, (on == FAN_ACTIVE_HIGH) ? HIGH : LOW);
}

static const char *statusName(fan::TempStatus s) {
    switch (s) {
    case fan::TempStatus::Valid:          return "OK";
    case fan::TempStatus::Disconnected:   return "DISCONNECTED";
    case fan::TempStatus::PowerOnDefault: return "85C-DEFAULT";
    }
    return "?";
}

void setup() {
    // FAILSAFE FIRST: latch the OFF level, THEN enable the output. Writing the
    // latch before pinMode() means the fan never glitches on at boot even on an
    // active-low module.
    fanWrite(false);
    pinMode(PIN_FAN_MOSFET, OUTPUT);
    fanWrite(false);

    Serial.begin(115200);
    delay(200);
    Serial.println("\n[moto-control] boot");
#ifdef BENCH_MODE
    const char *buildMode = "BENCH";
#else
    const char *buildMode = "PRODUCTION";
#endif
    Serial.printf("[moto-control] mode=%s  fan ON=%.0fC OFF=%.0fC\n",
                  buildMode, FAN_ON_C, FAN_OFF_C);

    ds18b20.begin();
    ds18b20.setWaitForConversion(true);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    adsOk = ads.begin(0x48, &Wire);
    // SPI has no ACK, so unlike the I2C path there is no way to detect whether a
    // display is actually connected — begin() only tells us the framebuffer
    // malloc succeeded. oledOk therefore means "driver initialised", not
    // "hardware present" (see docs/DECISIONS.md D4).
    oledOk = display.begin(SSD1306_SWITCHCAPVCC);
    Serial.printf("[moto-control] ADS1115:%s  OLED:%s\n",
                  adsOk ? "present" : "ABSENT", oledOk ? "present" : "ABSENT");

    // Seed motohours from flash so the meter survives power cycles.
    prefs.begin("moto", false);
    uint32_t savedSeconds = prefs.getUInt("hours_s", 0);
    hourMeter = engine::HourMeter({RUN_ON_V, RUN_OFF_V}, savedSeconds);
    Serial.printf("[moto-control] restored motohours: %.2f h\n", hourMeter.hours());

    lastLoopMs = millis();
    lastNvsMs = millis();
}

static float readBatteryVolts() {
    if (!adsOk) return 0.0f;
    int16_t raw = ads.readADC_SingleEnded(ADS_CHANNEL);
    return ads.computeVolts(raw) * VDIV_RATIO;
}

static void renderOled(const fan::TempReading &t, bool fanOn, float volts) {
    if (!oledOk) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    if (t.status == fan::TempStatus::Valid) {
        display.printf("Temp: %.1f C\n", t.celsius);
    } else {
        display.printf("Temp: %s\n", statusName(t.status));
    }
    display.printf("Fan : %s\n", fanOn ? "ON" : "off");
    display.printf("Batt: %.1f V\n", volts);
    display.printf("Run : %s\n", hourMeter.running() ? "yes" : "no");
    display.printf("Hrs : %.2f\n", hourMeter.hours());
    display.display();
}

void loop() {
    uint32_t now = millis();
    if (now - lastLoopMs < LOOP_INTERVAL_MS) return;
    uint32_t dt = now - lastLoopMs;
    lastLoopMs = now;

    // --- temperature -> fan decision -> MOSFET ---
    ds18b20.requestTemperatures();
    fan::TempReading reading = classifyTemp(ds18b20.getTempCByIndex(0));
    bool fanOn = fanCtl.update(reading);
    fanWrite(fanOn);

    // --- voltage -> motohours ---
    float volts = readBatteryVolts();
    hourMeter.update(volts, dt);

    // --- persist motohours: only if changed AND cadence elapsed (spare flash) ---
    if (hourMeter.dirty() && (now - lastNvsMs >= NVS_WRITE_MS)) {
        prefs.putUInt("hours_s", (uint32_t)hourMeter.seconds());
        hourMeter.markPersisted();
        lastNvsMs = now;
        Serial.println("[moto-control] motohours persisted");
    }

    // --- report ---
    Serial.printf("T=%.1f(%s) fan=%s V=%.2f run=%s hrs=%.2f\n",
                  reading.celsius, statusName(reading.status),
                  fanOn ? "ON" : "off", volts,
                  hourMeter.running() ? "Y" : "N", hourMeter.hours());

    renderOled(reading, fanOn, volts);
}
