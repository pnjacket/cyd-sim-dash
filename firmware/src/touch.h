// COMPONENT-TOUCH — the XPT2046 resistive touch controller.
//
// Reports one fact: is the glass being pressed. No coordinates, because the product has exactly one
// touch action and it is available anywhere on the panel. Reading a position we would then ignore
// would be three more SPI transactions per poll and a calibration step nobody needs.
//
// This is the second place hardware pins are named; the display owns the other. Both sets are on
// this header's terms rather than a library's, for the same reason tft_config.h exists.

#ifndef CYD_TOUCH_H
#define CYD_TOUCH_H

#include <stdint.h>

namespace cyd {
namespace touch {

// The touch controller's own SPI pins.
//
// These are NOT the display's. The ESP32-2432S028R routes the XPT2046 to a second set of pins and a
// second SPI peripheral — see ADR-TOUCH-OWN-BUS. Assigning any of these to the display bus is the
// documented way to get touch that works intermittently or not at all.
constexpr int kSck  = 25;
constexpr int kMiso = 39;   // input-only pin, which is all a MISO line needs
constexpr int kMosi = 32;
constexpr int kCs   = 33;

// The XPT2046 tolerates a few MHz, nothing like the display's 80. Its own bus means this is simply
// the bus speed rather than something reconfigured around each read.
constexpr uint32_t kSpiHz = 2000000;

// Pressure floor for a real press. The controller reports a resistance that falls as contact area
// and force rise, so this is a threshold on the derived value rather than on a raw channel. Below it
// are the readings a bare panel produces from noise alone.
constexpr int kPressureFloor = 400;

// A press must be held this long to count, and the glass must be released for this long before
// another press is reported.
//
// Deliberately slack: the two-touch sequence is the only consumer and it is a deliberate act, not a
// game control. Generous debouncing here costs nothing the operator can perceive and rules out the
// sleeve-brush that the two-touch rule is already guarding against.
constexpr uint32_t kHoldMs    = 60;
constexpr uint32_t kReleaseMs = 250;

// Brings up the touch bus. Call once, after the display — the display's begin() configures its own
// peripheral, and doing this first has no effect on that but keeps the ordering obvious.
void begin();

// Poll. Returns true exactly once per press, on the transition into a held press: a press that is
// held for a minute reports once, not for as long as it lasts. Cheap enough to call every loop.
bool pressed(uint32_t nowMs);

// The raw pressure of the most recent poll. Exposed only so a bring-up run can tell "the wiring is
// wrong" from "the threshold is wrong" — a dead bus reads a constant, and a live one moves.
int lastPressure();

}  // namespace touch
}  // namespace cyd

#endif  // CYD_TOUCH_H
