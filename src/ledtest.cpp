// Stage 1 bench helper — output-path isolation test.
// Blinks the REAL fan/LED pin (PIN_FAN_MOSFET from config.h) at 1 Hz, respecting
// FAN_ACTIVE_HIGH polarity. Proves GPIO4 -> 330R -> LED -> GND wiring works
// BEFORE the DS18B20 is added, so a dark LED later means logic/sensor, not wiring.
//
// Flash with:  .venv/bin/pio run -e ledtest -t upload
// Not part of the production firmware (excluded from every real build via
// build_src_filter in platformio.ini). Restore normal behaviour by flashing
// esp32dev_bench again.
#include <Arduino.h>
#include "config.h"

static void driveFan(bool on) {
    digitalWrite(PIN_FAN_MOSFET, (on == FAN_ACTIVE_HIGH) ? HIGH : LOW);
}

void setup() {
    driveFan(false);                    // failsafe: off before enabling the pin
    pinMode(PIN_FAN_MOSFET, OUTPUT);
    driveFan(false);
    Serial.begin(115200);
    delay(200);
    Serial.printf("[ledtest] blinking GPIO%d at 1 Hz (active_high=%d)\n",
                  PIN_FAN_MOSFET, FAN_ACTIVE_HIGH);
}

void loop() {
    driveFan(true);
    Serial.println("[ledtest] LED ON");
    delay(500);
    driveFan(false);
    Serial.println("[ledtest] LED OFF");
    delay(500);
}
