// EngineHours — pure motohours accumulator. NO Arduino.h, NO millis().
// The elapsed time between updates is PASSED IN (dtMillis), so the meter is
// deterministic and host-testable: feed it a voltage trace + time steps and
// assert the accumulated total.
//
// How "engine running" is detected (from the project handoff):
//   The CRF250L has an always-on headlight. Key-on/engine-off sags the system
//   to ~12.0-12.4 V; a running engine's charging system climbs to ~14.0-14.5 V.
//   So we treat voltage as a proxy for "running" via hysteresis:
//     >= runOnV  (~13.2 V) -> running
//     <= runOffV (~12.8 V) -> stopped
//   The dead band stops a voltage hovering near threshold from flickering the
//   running state (and thus the accumulator) on and off.
//
// Persistence: the meter tracks whether its total has changed since the last
// markPersisted(), so the hardware layer can skip NVS/flash writes while the
// engine is off (total static) instead of burning flash-erase cycles at idle.
#pragma once

#include <cstdint>

namespace engine {

struct HoursConfig {
    float runOnV;   // >= this system voltage => engine considered running
    float runOffV;  // <= this system voltage => engine considered stopped
};

class HourMeter {
public:
    // initialSeconds seeds the accumulated total on boot (read from NVS by the
    // hardware layer). Starts "not running" until voltage says otherwise.
    explicit HourMeter(HoursConfig cfg, uint32_t initialSeconds = 0)
        : cfg_(cfg),
          runMillis_(static_cast<uint64_t>(initialSeconds) * 1000ULL),
          persistedMillis_(runMillis_) {}

    // Advance the meter. `voltage` is the current system voltage; `dtMillis` is
    // the elapsed time since the previous update() call. The elapsed interval is
    // credited to whatever run-state was in effect DURING it (i.e. before this
    // reading changes the state). Returns the running state after this update.
    bool update(float voltage, uint32_t dtMillis);

    bool running() const { return running_; }
    uint64_t seconds() const { return runMillis_ / 1000ULL; }
    double hours() const { return static_cast<double>(runMillis_) / 3600000.0; }

    // Persistence helpers for the hardware layer.
    bool dirty() const { return runMillis_ != persistedMillis_; }
    void markPersisted() { persistedMillis_ = runMillis_; }

private:
    HoursConfig cfg_;
    uint64_t runMillis_;        // total accumulated running time, milliseconds
    uint64_t persistedMillis_;  // total as of the last markPersisted()
    bool running_ = false;
};

} // namespace engine
