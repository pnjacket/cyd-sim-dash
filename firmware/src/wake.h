// The wake sequence and the magic packet — the pure half of CAP-WAKE-RIG.
//
// Free of Arduino, WiFi and TFT headers, for the same reason display_state.h is: the two-touch rule
// and the packet layout are both worth asserting on the host, and neither needs hardware to be
// right. What is left for the device is reading the glass and putting 102 bytes on the wire.
//
// Contracts realised here: the two-touch rule of CAP-WAKE-RIG, the payload of EVT-WAKE, and
// INV-WAKE-NEEDS-LEARNED-MAC.

#ifndef CYD_WAKE_H
#define CYD_WAKE_H

#include <stddef.h>
#include <stdint.h>

#include "display_state.h"

namespace cyd {
namespace wake {

// EVT-WAKE: six 0xFF octets, then the target address sixteen times.
constexpr size_t kMagicPacketBytes = 6 + 16 * 6;   // 102

// How long the offer stands after the first touch.
//
// Ten seconds, and the number is a judgement rather than a measurement. The operator has just
// touched a panel that was dark, has to read a line that was not there a moment ago, and then
// decide; two seconds would make the second touch a reflex test. Much longer and the panel is
// sitting armed long after whoever touched it walked away, which is the state the two-touch rule
// exists to avoid.
constexpr uint32_t kOfferWindowMs = 10000;

// Fills `out` with the magic packet for `mac`. Returns the number of octets written, or 0 if the
// buffer is too small — never a partial packet, which on this wire would be indistinguishable from
// a valid one for a different machine.
size_t buildMagicPacket(const uint8_t mac[6], uint8_t* out, size_t cap);

// What a touch did. All four are distinct outcomes the caller must handle differently, which is why
// this is not a bool: `Lit` and `Offered` look the same on the wire and completely different on the
// glass, and telling them apart is the whole of INV-WAKE-NEEDS-LEARNED-MAC.
enum class TouchAction : uint8_t {
  Lit,        // the panel lights and the blanking period restarts; nothing else
  Offered,    // as Lit, plus the offer is now on screen and a second touch would send
  Sent,       // the packet goes out now
};

// The two-touch sequence. One instance, owned by the loop.
class Sequence {
 public:
  // Call for every debounced press. `link` is the ladder's current answer and `macKnown` is whether
  // ENTITY-RIGADDRESS holds an address.
  //
  // A touch ALWAYS lights the panel, in every state, which is why there is no Ignored outcome: that
  // is what makes a blanked panel usable at all, and it is the only way the backlight comes on other
  // than a live frame.
  TouchAction onTouch(uint32_t nowMs, LinkState link, bool macKnown);

  // Lapses a standing offer. Call every loop.
  void tick(uint32_t nowMs);

  bool armed() const { return armed_; }

 private:
  bool     armed_     = false;
  uint32_t armedAtMs_ = 0;
};

}  // namespace wake
}  // namespace cyd

#endif  // CYD_WAKE_H
