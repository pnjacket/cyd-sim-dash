// Display-state engine — the pure logic of the panel.
//
// Realises COMPONENT-STATE. Deliberately free of Arduino, WiFi, JSON and TFT headers so it
// compiles for the host and its unit tier runs in CI with nothing but a C++ compiler. That is
// also what lets E2E-STANDARD clause 6 import it to compute expectations.
//
// It contains NO per-title logic. Everything sim-specific was resolved by a title adapter on the
// PC before the frame crossed the wire, and COMPONENT-STATE's "no per-title branch" forbids
// reintroducing it here. (This cited INV-NO-TITLE-BRANCH until 2026-09-23 - an ID no document
// defines. Code does not get to mint contracts.)
// If something in this file ever needs to know which sim is running, the adapter boundary has
// leaked.
//
// Contracts realised here: ENTITY-DISPLAYSTATE, INV-STAMP-ORDER, INV-FRESH-RENDER,
// INV-GEAR-DOMAIN, INV-RAMP-ORDER, INV-RPM-NONNEG, INV-VERSION-WHOLE, INV-STATUS-CONSISTENT,
// and the linkState derivation ladder owned by Domain & Data.

#ifndef CYD_DISPLAY_STATE_H
#define CYD_DISPLAY_STATE_H

#include <stdint.h>
#include <stddef.h>

namespace cyd {

// ---------------------------------------------------------------------------
// Protocol and timing constants. Every one of these is a contract value; none
// is a tuning knob to be nudged without amending the doc that owns it.
// ---------------------------------------------------------------------------

constexpr uint16_t kProtocolMajor = 1;  // same major means compatible
constexpr uint16_t kProtocolMinor = 0;  // differing minor tolerated, unknown fields ignored

constexpr uint32_t kStalenessMs      = 2000;  // Domain & Data: INV-FRESH-RENDER
constexpr uint32_t kFirstFrameGraceMs = 5000; // drivingPending -> unreachable boundary
constexpr uint32_t kRegisterIntervalMs = 2000;
constexpr uint32_t kDeviceForgottenMs  = 6000;

constexpr size_t kMaxDatagramBytes = 2048;  // SEC-INPUT-BOUND: rejected before parsing

// ---------------------------------------------------------------------------
// Wire-facing value types. These mirror EVT-FRAME exactly.
// ---------------------------------------------------------------------------

enum class Status : uint8_t { Live, NoSim, UnsupportedTitle, AdapterFault };

// A side's proximity. Unavailable is NOT the same fact as None: one says the title cannot tell
// you, the other says the track is clear. Collapsing them is how a dark bar comes to look
// identical whether it is working or broken.
enum class Spotter : uint8_t { Unavailable, None, One, Two };

struct Frame {
  uint16_t protocolMajor = 0;
  uint16_t protocolMinor = 0;
  Status   status        = Status::NoSim;

  // titleId is NOT a telemetry element; INV-STATUS-CONSISTENT exempts it, because naming the
  // title is the entire point of the unsupportedTitle status.
  char     titleId[32]   = {0};

  uint32_t stamp         = 0;

  bool     gearPresent   = false;
  char     gear[3]       = {0};   // "R", "N", "1".."18"

  bool     rpmPresent    = false;
  float    rpm           = 0.0f;

  bool     rampPresent   = false;
  float    rampStartRpm  = 0.0f;

  bool     flashPresent  = false;
  float    flashRpm      = 0.0f;

  Spotter  spotterLeft   = Spotter::Unavailable;
  Spotter  spotterRight  = Spotter::Unavailable;
};

// ---------------------------------------------------------------------------
// Display state — what the panel should show. Derived, never transmitted,
// never persisted.
// ---------------------------------------------------------------------------

enum class ShiftPhase : uint8_t { Unavailable, Neutral, Ramping, Flashing };

// The nine link conditions, plus Driving. Driving serialises to null over API-STATE: the ladder
// explicitly produces no link state when a fresh live frame is in hand.
enum class LinkState : uint8_t {
  Driving,
  DrivingPending,
  Joining,
  Unresolved,
  Unreachable,
  Stale,
  NoSim,
  UnsupportedTitle,
  AdapterFault,
  VersionMismatch,
};

struct DisplayState {
  bool       gearPresent  = false;
  char       gearGlyph[3] = {0};
  ShiftPhase shift        = ShiftPhase::Unavailable;
  float      rampPosition = 0.0f;   // 0..1, meaningful only when shift == Ramping
  bool       barLeft      = false;
  bool       barRight     = false;
  LinkState  link         = LinkState::Joining;
};

// ---------------------------------------------------------------------------
// Frame acceptance
// ---------------------------------------------------------------------------

enum class Reject : uint8_t {
  Accepted,
  VersionMajor,     // ERR-VERSION-MAJOR  - hard, its own screen
  FieldRange,       // ERR-FIELD-RANGE    - soft, counted
  StatusInconsistent,
  OutOfOrder,       // ERR-OUT-OF-ORDER   - soft, counted
};

// Tracks stamp ordering across a producer run.
//
// The subtlety worth keeping: when SimHub restarts, its stamp counter resets to near zero. A naive
// "newest wins" comparison would then reject every subsequent frame forever, leaving a dead panel
// with nothing on screen to explain it. A backwards jump larger than the staleness threshold is
// therefore read as a new producer run rather than as reordering.
class StampTracker {
 public:
  bool accept(uint32_t stamp);
  void reset();
  bool seen() const { return seen_; }
  uint32_t newest() const { return newest_; }

 private:
  bool     seen_   = false;
  uint32_t newest_ = 0;
};

// Validates a frame against the invariants that do not depend on history.
Reject validate(const Frame& f);

// Full acceptance: validation plus ordering. Mutates the tracker only on acceptance.
Reject acceptFrame(const Frame& f, StampTracker& tracker);

// ---------------------------------------------------------------------------
// Link inputs and derivation
// ---------------------------------------------------------------------------

struct LinkInputs {
  bool     wifiAssociated    = false;
  bool     hostResolved      = false;
  bool     everAccepted      = false;  // any frame accepted this session
  bool     versionRejected   = false;  // newest datagram refused for major, nothing accepted since
  uint32_t msSinceRegister   = 0;      // since the first registration was sent
  uint32_t msSinceAccepted   = 0;      // since the newest accepted frame; meaningless if !everAccepted
};

// The linkState ladder, first match wins. Ordering is the substance: link-layer facts outrank
// frame contents, and freshness outranks whatever a stale frame happened to say.
LinkState deriveLink(const LinkInputs& in, Status newestStatus);

// Derives the full display state. `newest` is only read when the ladder yields Driving.
DisplayState derive(const Frame& newest, const LinkInputs& in);

// Ramp position for an rpm within [rampStart, flash]. Clamped to 0..1.
float rampPosition(float rpm, float rampStart, float flash);

// 3 Hz, 50% duty: a 333 ms period, red for the first half.
//
// The rate is a deliberate safety choice rather than a default. Full-field flashing between roughly
// 3 and 60 Hz is the range associated with photosensitive seizures, and general guidance caps
// large-area flashing at three per second. The product may be shared publicly, so the slower rate
// is taken even though a faster one would read as more urgent.
constexpr uint32_t kFlashPeriodMs = 333;


// True while the red half of the flash is showing.
//
// Lives in the engine rather than in the renderer for two reasons: it is pure, so the unit tier can
// assert the cadence with no panel and no device; and deriving it from the clock rather than
// toggling a flag means the rate cannot drift with however long a frame took to draw, and a
// dropped frame shifts nothing.
bool flashOn(uint32_t nowMs);

// True when the gear string is inside INV-GEAR-DOMAIN.
bool gearInDomain(const char* gear);

}  // namespace cyd

#endif  // CYD_DISPLAY_STATE_H
