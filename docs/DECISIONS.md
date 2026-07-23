# Decision Log — CRF250L Fan Controller

Lightweight ADR-style record of design decisions that aren't obvious from the code.
Newest at the bottom. Each entry: context → decision → consequences → any deferred
alternative worth revisiting.

---

## D1 — DS18B20 sensor-fault policy: defer to OEM

**Date:** 2026-07-15 · **Status:** Accepted · **Code:** `lib/FanControl/FanControl.cpp` (`decideOnFault`)

### Context

The DS18B20 has two reads that must not be treated as real temperatures:
- **-127 °C** — disconnected / CRC failure (`DEVICE_DISCONNECTED_C`).
- **exactly 85.0 °C** — the sensor's power-on-reset register default. It looks
  plausible *and* sits inside our ON band (~88 °C ON), so a read taken before the
  first conversion completes (or after a power glitch) can spuriously look "hot".

`FanControl` receives a `TempReading` (value + `TempStatus`), never a bare float,
so the fault case is explicit. The question: what should our fan path command
when the reading can't be trusted?

Key de-risking fact: our path is wired **in parallel** with the OEM thermoswitch
(~98 °C), which is fully independent. The engine therefore cannot overheat
undetected regardless of what `decideOnFault` returns — OEM is the hard backstop.
The choice only governs behaviour in the ~88–98 °C gap while the sensor is lying.

### Decision

**Policy B — defer to OEM.** On any untrusted reading, our path abstains
(`return false`). Because we're parallel to OEM, "false" means "abstain", not
"force the fan off" — OEM can still run the fan on its own. The bike degrades to
exactly stock behaviour whenever our sensor is unreliable.

### Consequences

- ✅ Purest "additive, never subtractive": a fault reverts us to stock, easy to reason about.
- ✅ No spurious fan running (no battery drain / wear when servicing with the probe unplugged).
- ✅ One line, one obvious behaviour — minimal surface to get wrong (simplicity as a safety property).
- ⚠️ Loses early-engagement while the fault persists: on slow technical riding with a
  dead sensor, coolant can climb toward OEM's higher 98 °C trip before the fan helps.
  Accepted because a dead sensor is a bug to fix, not a steady state to optimise for,
  and OEM still caps the peak.

### Rejected alternatives

- **A — Fail ON** (`return true`): run the fan on any untrusted reading. Rejected:
  can't distinguish "engine hot" from "sensor unplugged", so the fan runs continuously
  whenever the probe is disconnected (e.g. mid-service) — a real daily annoyance for a
  benefit OEM largely already provides.

### Deferred for later review

- **C — Branch on `status`** (revisit once we have field data). Treat the two faults
  by their meaning: hold last state on a *transient* 85 °C (single bad read at boot /
  glitch), but escalate a *persistent* fault. Doing this honestly requires **memory** —
  a `faultStreak_` counter incremented on fault and reset on a good read, escalating
  from "hold" to a chosen action past a threshold. Stays fully host-testable (feed N
  consecutive fault readings, assert escalation). Deferred because it adds complexity to
  solve a problem we haven't measured yet; decide after observing how often transient
  faults actually occur on the bike.

---

## D2 — Engine-running detection: raw voltage hysteresis (no debounce)

**Date:** 2026-07-15 · **Status:** Accepted · **Code:** `lib/EngineHours/EngineHours.cpp`

### Context

Motohours are accumulated while the engine is "running", detected from system
voltage (charging pulls it to ~14 V; key-on/engine-off sags to ~12 V). Detection
uses hysteresis: running >= 13.2 V, stopped <= 12.8 V.

### Decision

**Raw hysteresis, no debounce.** The instant voltage crosses the ON threshold the
engine counts as running. Simple and adequate: motohours don't need second-level
precision, and the dead band already rejects flicker around a single threshold.

### Consequences

- ✅ Minimal, one-shape-fits-both logic (same hysteresis as the fan controller).
- ⚠️ A hard transient — a throttle blip at standstill, or an electrical spike —
  that briefly exceeds 13.2 V could be counted as a moment of "running". Bounded
  to at most one loop interval (~1 s) per event; negligible over real motohours.

### Deferred for later review

- **Debounce** — require voltage sustained above the ON threshold for N seconds
  (e.g. 2-3 s) before counting as running, to reject transients. Adds a timer/
  counter to the meter's state; stays host-testable. Deferred for the same reason
  as fault-policy option C (D1): complexity solving an unmeasured problem. Revisit
  if field data shows transient over-counting matters.

---

## D3 — Fan MOSFET drive polarity: assumed active-HIGH (UNVERIFIED gate)

**Date:** 2026-07-15 · **Status:** Assumed, pending bench verification · **Code:** `include/config.h` (`FAN_ACTIVE_HIGH`), `src/main.cpp` (`fanWrite`)

### Context

The entire failsafe rests on one physical fact: **signal HIGH energises the fan,
LOW = off.** On that basis, boot drives the pin to the OFF level and policy-B
(defer to OEM) commands OFF on a fault. But opto-isolated MOSFET/relay boards are
frequently **active-LOW** (signal LOW energises the load). If this LR7843 module
is active-low, both invariants invert: boot would command the fan ON, and a
disconnected sensor would RUN the fan instead of deferring. Native tests can't
catch this — they assert the boolean, not what the pin does to the module.

### Decision

Assume **active-HIGH** for now, but make it a single explicit constant
(`FAN_ACTIVE_HIGH`) routed through one helper (`fanWrite`), so correcting an
active-low module is a one-line flip. Latch the OFF level before `pinMode(OUTPUT)`
so there is no boot glitch to the ON level regardless of polarity.

### Verification gate (BEFORE trusting on the bike)

On the bench, with the module driving the fan (or a test load) on a separate
supply: confirm that `fanWrite(true)` / a HIGH gate actually turns the load ON and
`fanWrite(false)` / LOW turns it OFF — scope or meter the gate, or just watch the
load. Do this alongside the OEM thermoswitch ground-side multimeter check. If it's
active-low, set `FAN_ACTIVE_HIGH = false` and re-verify boot-off.

### Also fixed here

OLED presence detection: `Adafruit_SSD1306::begin()` returns true on framebuffer
malloc success without checking an I2C ACK, so it always reported "present". Now
probed with an explicit `Wire.beginTransmission/endTransmission` ACK test first.


