#include "FanControl.h"

namespace fan {

// Hysteresis. The dead band (offC < c < onC) holds whatever state we were in,
// so the fan doesn't chatter when the coolant hovers near a single threshold.
//   - currently ON  -> stay on until we drop to/below offC
//   - currently OFF -> start only once we reach onC
bool Controller::decideValid(float celsius) const {
    if (on_) {
        return celsius > t_.offC;
    }
    return celsius >= t_.onC;
}

bool Controller::update(const TempReading &r) {
    switch (r.status) {
    case TempStatus::Valid:
        on_ = decideValid(r.celsius);
        break;
    default: // Disconnected or PowerOnDefault — reading not trustworthy
        on_ = decideOnFault(r.status);
        break;
    }
    return on_;
}

// Sensor-fault policy: DEFER TO OEM  (decision D1 — see docs/DECISIONS.md).
//
// When the reading can't be trusted, our path abstains so the bike behaves
// exactly as stock — the OEM thermoswitch still cools at ~98 C. Because we are
// wired in PARALLEL with OEM, returning false means "abstain", NOT "force the
// fan off": OEM can still pull the fan ground independently.
//
// Deliberately treats every untrusted reading the same. The per-fault-type
// branching alternative (option C: hold on a transient 85 C, escalate a
// persistent Disconnect via a fault-streak counter) is recorded in D1 for
// review once we have field data on how often transient faults actually occur.
bool Controller::decideOnFault(TempStatus status) const {
    (void)status;   // policy B does not distinguish fault kinds
    return false;   // abstain — defer cooling to the independent OEM thermoswitch
}

} // namespace fan
