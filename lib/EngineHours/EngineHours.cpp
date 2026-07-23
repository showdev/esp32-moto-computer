#include "EngineHours.h"

namespace engine {

bool HourMeter::update(float voltage, uint32_t dtMillis) {
    // Credit the just-elapsed interval to the state we believed during it,
    // BEFORE this new reading is allowed to change the state. This keeps the
    // accumulation correct across start/stop transitions (the interval that
    // ends a run still counts; the interval that starts one does not).
    if (running_) {
        runMillis_ += dtMillis;
    }

    // Recompute running-state with hysteresis (same dead-band shape as the fan
    // controller): only cross a threshold, never chatter inside the band.
    if (running_) {
        running_ = voltage > cfg_.runOffV; // stay running until we sag to runOffV
    } else {
        running_ = voltage >= cfg_.runOnV; // start only once we reach runOnV
    }

    return running_;
}

} // namespace engine
