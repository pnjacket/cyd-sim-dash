#include "display_state.h"

#include <string.h>

namespace cyd {
namespace {

bool isTelemetryNull(const Frame& f) {
  // titleId is deliberately excluded: INV-STATUS-CONSISTENT exempts it.
  return !f.gearPresent && !f.rpmPresent && !f.rampPresent && !f.flashPresent &&
         f.spotterLeft == Spotter::Unavailable && f.spotterRight == Spotter::Unavailable;
}

bool barLit(Spotter s) {
  // Unavailable and None both render dark, but they remain distinct in the data so the display
  // can later choose to present them differently without a wire change.
  return s == Spotter::One || s == Spotter::Two;
}

}  // namespace

bool gearInDomain(const char* gear) {
  if (gear == nullptr) return false;
  const size_t n = strlen(gear);
  if (n == 0 || n > 2) return false;
  if (n == 1) {
    if (gear[0] == 'R' || gear[0] == 'N') return true;
    return gear[0] >= '1' && gear[0] <= '9';
  }
  // two characters: 10..18 only
  if (gear[0] != '1') return false;
  return gear[1] >= '0' && gear[1] <= '8';
}

float rampPosition(float rpm, float rampStart, float flash) {
  if (flash <= rampStart) return 0.0f;      // guarded by INV-RAMP-ORDER upstream
  const float p = (rpm - rampStart) / (flash - rampStart);
  if (p < 0.0f) return 0.0f;
  if (p > 1.0f) return 1.0f;
  return p;
}

Reject validate(const Frame& f) {
  // INV-VERSION-WHOLE: a frame whose major we do not accept is discarded entire, never partially
  // decoded. Checked first, before any field is trusted.
  if (f.protocolMajor != kProtocolMajor) return Reject::VersionMajor;

  // INV-STATUS-CONSISTENT
  if (f.status != Status::Live) {
    if (!isTelemetryNull(f)) return Reject::StatusInconsistent;
    return Reject::Accepted;
  }

  // INV-RPM-NONNEG
  if (f.rpmPresent && !(f.rpm >= 0.0f)) return Reject::FieldRange;

  // INV-GEAR-DOMAIN
  if (f.gearPresent && !gearInDomain(f.gear)) return Reject::FieldRange;

  // INV-RAMP-ORDER: strictly less than. Equal thresholds would make the ramp a division by zero
  // and the flash point ambiguous, so equality is a violation rather than a degenerate case.
  if (f.rampPresent != f.flashPresent) return Reject::FieldRange;
  if (f.rampPresent && f.flashPresent && !(f.rampStartRpm < f.flashRpm)) return Reject::FieldRange;

  return Reject::Accepted;
}

void StampTracker::reset() {
  seen_ = false;
  newest_ = 0;
}

bool StampTracker::accept(uint32_t stamp) {
  if (!seen_) {
    seen_ = true;
    newest_ = stamp;
    return true;
  }
  if (stamp > newest_) {
    newest_ = stamp;
    return true;
  }
  // Backwards. A jump larger than the staleness threshold is a new producer run — SimHub
  // restarted and its counter went back to near zero. Without this, the device would reject every
  // subsequent frame forever and the panel would simply die.
  const uint32_t backwards = newest_ - stamp;
  if (backwards > kStalenessMs) {
    newest_ = stamp;
    return true;
  }
  return false;  // genuine reordering, or a duplicate
}

Reject acceptFrame(const Frame& f, StampTracker& tracker) {
  const Reject v = validate(f);
  if (v != Reject::Accepted) return v;
  if (!tracker.accept(f.stamp)) return Reject::OutOfOrder;
  return Reject::Accepted;
}

LinkState deriveLink(const LinkInputs& in, Status newestStatus) {
  // First match wins. The order is the contract.
  if (!in.wifiAssociated)                       return LinkState::Joining;          // 1
  if (!in.hostResolved)                         return LinkState::Unresolved;       // 2
  if (in.versionRejected)                       return LinkState::VersionMismatch;  // 3

  if (!in.everAccepted) {
    return (in.msSinceRegister < kFirstFrameGraceMs)
               ? LinkState::DrivingPending                                          // 4
               : LinkState::Unreachable;                                            // 5
  }

  // 6 — freshness outranks what a stale frame happened to say. A four-second-old adapterFault
  // reads as Stale, because an old fault tells you nothing about now.
  if (in.msSinceAccepted > kStalenessMs)        return LinkState::Stale;

  switch (newestStatus) {                                                           // 7, 8, 9
    case Status::NoSim:            return LinkState::NoSim;
    case Status::UnsupportedTitle: return LinkState::UnsupportedTitle;
    case Status::AdapterFault:     return LinkState::AdapterFault;
    case Status::Live:             return LinkState::Driving;  // serialises to null
  }
  return LinkState::Driving;
}

DisplayState derive(const Frame& newest, const LinkInputs& in) {
  DisplayState s;
  s.link = deriveLink(in, newest.status);

  // INV-FRESH-RENDER: the driving screen renders only from a fresh live frame. Anything else and
  // the panel shows link state instead of asserting values it cannot stand behind.
  if (s.link != LinkState::Driving) return s;

  if (newest.gearPresent && gearInDomain(newest.gear)) {
    s.gearPresent = true;
    strncpy(s.gearGlyph, newest.gear, sizeof(s.gearGlyph) - 1);
  }

  s.barLeft  = barLit(newest.spotterLeft);
  s.barRight = barLit(newest.spotterRight);

  if (!newest.rpmPresent || !newest.rampPresent || !newest.flashPresent) {
    s.shift = ShiftPhase::Unavailable;
    return s;
  }

  if (newest.rpm >= newest.flashRpm) {
    s.shift = ShiftPhase::Flashing;      // continues while RPM stays there; no over-rev state
    s.rampPosition = 1.0f;
  } else if (newest.rpm >= newest.rampStartRpm) {
    s.shift = ShiftPhase::Ramping;
    s.rampPosition = rampPosition(newest.rpm, newest.rampStartRpm, newest.flashRpm);
  } else {
    s.shift = ShiftPhase::Neutral;       // pure black, most of your driving time
    s.rampPosition = 0.0f;
  }
  return s;
}

bool flashOn(uint32_t nowMs) {
  return (nowMs % kFlashPeriodMs) < (kFlashPeriodMs / 2);
}

}  // namespace cyd
