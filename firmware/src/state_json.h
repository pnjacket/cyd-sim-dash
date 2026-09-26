// The API-STATE JSON projection. Host-compilable: no Arduino networking headers.

#ifndef CYD_STATE_JSON_H
#define CYD_STATE_JSON_H

#include "display_state.h"
#include "net_counters.h"

namespace cyd {
namespace stateapi {

/// Everything the endpoint reports that is not already in DisplayState.
struct Context {
  const char* deviceId = "";
  const char* firmwareVersion = "";
  const char* configuredHost = "";
  bool        linkStateApplies = false;   // false while the driving screen is showing
  LinkState   linkState = LinkState::Driving;
  bool        lastFrameAgePresent = false;
  uint32_t    lastFrameAgeMs = 0;
  uint32_t    lastDrawUs = 0;
  uint32_t    worstDrawUs = 0;
  uint32_t    drawCount = 0;

  // How long since the last boot, and why that boot happened. Together these answer "did the panel
  // restart, and was it me?" - a question that otherwise can only be inferred from counters
  // resetting, which is guesswork. resetReason distinguishes an intended restart (a firmware update
  // or a saved configuration) from a crash, a watchdog, or a brownout. The last matters on this
  // hardware: a CYD on marginal USB power can brown out under load, and that looks exactly like a
  // spontaneous reboot.
  uint32_t    uptimeMs = 0;
  const char* resetReason = "";

  // CAP-BLANK. backlightOn is what distinguishes a blanked panel from a dead one, which is the
  // whole mitigation for the trade-off that capability accepts - and it is reachable while the glass
  // is dark, which is exactly when it is needed.
  bool        backlightOn = true;
  uint16_t    blankAfterMinutes = 0;

  // CAP-WAKE-RIG. Both fields exist because the capability fails SILENTLY when no address has been
  // learned: there is no error path and nothing on the glass, so "the panel will not offer this" and
  // "the rig is ignoring Wake-on-LAN" look identical from the seat. This is what tells them apart.
  bool        wakeArmed = false;
  bool        rigMacKnown = false;
};

/// The buffer the projection is rendered into.
///
/// Measured rather than guessed: test_state_json.cpp renders every field at its maximum - including
/// a 63-character host of nothing but quotes, which doubles under escaping - and asserts both that
/// it fits and that at least 96 bytes of headroom remain. Widen this rather than trimming that test.
constexpr size_t kStateBufferBytes = 1024;

/// Render the JSON body. Returns characters written, excluding the NUL.
size_t render(char* out, size_t capacity,
              const DisplayState& state, const Context& context, const net::Counters& counters);

}  // namespace stateapi
}  // namespace cyd

#endif  // CYD_STATE_JSON_H
