# Bench Bring-Up Plan — staged hardware integration

Bring the system up one subsystem at a time, each with a signal you can trust
before adding the next. The firmware is already written; these stages attach
progressively more real hardware to the **same** GPIO4 output and I²C bus.

Key idea: **the indicator LED and the fan-on signal are the same GPIO4 output.**
`FanControl` decides → `fanWrite()` drives GPIO4 → whatever is attached responds.
Keep the LED wired through all stages as a permanent "fan commanded ON" debug light.

Flash the **`esp32dev_bench`** build for Stages 1–2 (fan thresholds ON 30 / OFF 28 °C,
hand-triggerable). Switch to **`esp32dev`** (production 88/80 °C) only for the bike.
The boot banner prints `mode=BENCH|PRODUCTION` so you always know what's flashed.

Pins (plain WROOM-32, from `include/config.h`):
| Signal | GPIO |
|--------|------|
| Fan MOSFET / LED | 4 |
| DS18B20 data | 5 (+ 4.7 kΩ pull-up to 3V3) |
| I²C SDA / SCL | 21 / 22 |

---

## Stage 1 — DS18B20 + LED (core sense→decide→act loop)

**Wire**
- DS18B20: data→GPIO5, Vcc→3V3, GND→GND, **4.7 kΩ pull-up between data and 3V3**.
  (Waterproof probe: red=Vcc, black=GND, yellow=data.)
- LED: GPIO4 → 330 Ω → LED anode → LED cathode → GND.

**Build/flash:** `.venv/bin/pio run -e esp32dev_bench -t upload`

**Verify**
- Serial shows a real, rising °C as you pinch/warm the probe (no longer -127).
- LED lights when temp crosses **30 °C**, stays on down to **28 °C**, then off —
  you're watching the hysteresis dead-band live.

**Watch for**
- GPIO5 is an ESP32 strapping pin (must be high at boot); the 4.7 kΩ pull-up
  holds it high, which matches — so the pull-up is required, not optional.
- Exactly 85.0 °C reads as the power-on-default fault; irrelevant at bench temps.

**Result (2026-07-24): PASSED.** Probe read a stable 23.4 °C (no longer -127 →
1-Wire + pull-up + boot all good). Output path isolated first via the `ledtest`
env (GPIO4 blink → LED, confirmed) before adding the sensor. Live run: held `off`
through 29.9, tripped **ON at 30.1 °C**, held ON through the dead-band down to
29.0, and was `off` + latched below 30 across 28.6→27.9. Exact 28.0 off-crossing
fell in a between-window gap (probe dipped <28 then rebounded); latch behaviour
confirmed, crossing itself covered by unit tests. Bench blink helper: `pio run -e
ledtest -t upload`; return to normal with `pio run -e esp32dev_bench -t upload`.

---

## Stage 2 — OLED temperature readout

The module on hand turned out to be the **SPI** SSD1306 (7-pin: D0/D1/RES/DC/CS),
not I²C — firmware was adapted to software SPI (see DECISIONS D4).

**Wire (SPI):** D0→GPIO18, D1→GPIO23, RES→GPIO25, DC→GPIO17, CS→GPIO16,
Vcc→3V3, GND→GND. (ADS1115 keeps I²C on GPIO21/22.)

**Firmware:** display constructor + init switched to software SPI; `renderOled()`
unchanged (draws temp/fan/volts/hours). Note: SPI has no ACK, so `OLED:present`
now means "driver initialised", not "hardware present" — verify visually.

**Verify:** on-screen text (`Temp/Fan/Batt/Run/Hrs`) matches serial.

**Result (2026-07-24): PASSED.** Boot banner `OLED:present`, five status lines on
the glass matching serial (`T=23.6 OK, fan off`). Live temp tracks the probe.

---

## Stage 3 — swap LED output for the real fan (D3 gate)

**Wire:** GPIO4 → LR7843 signal-in; ESP GND → module signal-GND. Load side: fan +
separate 12 V supply through the MOSFET; **1N4007 flyback across the fan terminals**.
Leave the LED on GPIO4 as the indicator (drives in parallel).

**Firmware:** none — but this is the **polarity verification gate** (DECISIONS D3):
confirm signal HIGH actually energises the fan. If inverted, set
`FAN_ACTIVE_HIGH = false` in `config.h` and re-verify boot-off.

**Verify:** warm probe → LED on AND fan spins; cool → both off. Do this alongside
the OEM thermoswitch ground-side multimeter check.

---

## After bench passes
- Calibrate: compare DS18B20 reading at the moment the OEM fan kicks in (~98 °C) to
  set the real offset; confirm production thresholds (88/80) engage earlier.
- Flash **`esp32dev`** (production) before install.
