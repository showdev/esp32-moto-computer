// FanControl — pure fan-decision logic. NO Arduino.h, NO millis(), NO bus reads.
// Everything it needs arrives as a function argument, so it compiles and runs on
// the host under `pio test -e native`. This is the failsafe-critical code, so it
// is the code that gets unit-tested in isolation.
//
// Context that shapes the design (from the project handoff):
//   * The ESP fan path is wired in PARALLEL with the OEM thermoswitch (~98 C).
//     The OEM switch is a fully independent backstop — if this logic does the
//     wrong thing or the ESP dies, the fan can still run on OEM alone.
//   * Our setpoints are deliberately LOWER than OEM (earlier engagement on slow
//     technical riding): ON ~88 C, OFF ~80 C, with a hysteresis band between.
#pragma once

namespace fan {

// How much to trust a temperature reading. The DS18B20 has two failure reads
// that must NOT be treated as real temperatures:
enum class TempStatus {
    Valid,          // trustworthy reading
    Disconnected,   // -127 C sentinel (DEVICE_DISCONNECTED_C) / CRC failure
    PowerOnDefault, // exactly 85.0 C — the sensor's reset-register value, which
                    // looks plausible AND sits inside our ON band. A read taken
                    // before the first conversion completes returns this.
};

// A temperature paired with how much we trust it. FanControl never sees a bare
// float — the hardware layer must classify the reading first.
struct TempReading {
    float celsius;
    TempStatus status;
};

// Hysteresis setpoints. offC < onC gives the dead band that stops the fan from
// chattering on/off around a single threshold.
struct Thresholds {
    float onC;   // command fan ON at or above this temperature
    float offC;  // command fan OFF at or below this temperature
};

class Controller {
public:
    explicit Controller(Thresholds t) : t_(t) {}

    // Feed one reading, get the desired fan state (true = ON) back.
    // Deterministic: same (previous state, reading) always yields the same result.
    bool update(const TempReading &r);

    bool fanOn() const { return on_; }

private:
    // Hysteresis decision for a TRUSTWORTHY reading.
    bool decideValid(float celsius) const;

    // Fault decision: what to command when the reading can't be trusted.
    // >>> Implemented by you in FanControl.cpp — see the TODO there. <<<
    bool decideOnFault(TempStatus status) const;

    Thresholds t_;
    bool on_ = false;  // last commanded state; starts OFF (fan not forced on at boot)
};

} // namespace fan
