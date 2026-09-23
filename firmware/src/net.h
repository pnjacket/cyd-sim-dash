// COMPONENT-NET — the device's side of the wire.
//
// Two jobs: announce this device to the PC every 2 s, and receive frames. It is the only place
// untrusted input enters the firmware, so the whole file is written on the assumption that anything
// on the LAN can send anything at all.
//
// SEC-INPUT-BOUND is structural rather than checked-after-the-fact: the receive buffer is a fixed
// 2 KB array allocated once, and a datagram larger than it is drained and counted WITHOUT being
// parsed. There is no dynamic allocation anywhere on this path, so a flood cannot fragment the heap
// on a device that has no way to recover from that except a reboot.
//
// SEC-PARSER-FLOOR is why DEP-ARDUINOJSON is pinned at >= 7.4.3: every version through 7.4.2 has a
// buffer overrun in string-to-float conversion reachable by a JSON string of many digits. The 2 KB
// cap does NOT close it, because 2 KB of digits is ample. The pin is the mitigation.

#ifndef CYD_NET_H
#define CYD_NET_H

#include <stdint.h>
#include "display_state.h"
#include "net_counters.h"

namespace cyd {
namespace net {

// The fixed port, agreed with the publisher. A fixed port is what keeps the PC side
// configuration-free: the device knows where to register without being told.
constexpr uint16_t kPort = 47110;

// Registration cadence and the PC-side expiry it is calibrated against. The PC forgets a device
// after 6 s, so 2 s means three announcements inside every expiry window: one may be lost, and a
// second may be lost, without the device ever going dark.
constexpr uint32_t kRegisterIntervalMs = 2000;

/// What the receive path reports upward after one poll.
struct PollResult {
  bool  frameAccepted = false;   // a good frame arrived and was newer than the last
  bool  versionRejected = false; // a frame with a different major arrived
  Frame frame;                   // valid only when frameAccepted
};

/// Bind the socket. Safe to call repeatedly; re-binds only if not already listening.
bool begin();

/// Send a registration if one is due. Cheap to call every loop.
void maintainRegistration(const char* deviceId, const char* firmwareVersion,
                          const char* pcHost, uint32_t nowMs);

/// Drain the socket, at most `maxDatagrams` per call so one noisy host cannot starve the renderer.
PollResult poll(StampTracker& stamps, Counters& counters, uint32_t nowMs, int maxDatagrams = 8);

/// Counters and diagnostics for API-STATE.
const Counters& counters();
uint32_t lastAcceptedAtMs();
bool everAccepted();

}  // namespace net
}  // namespace cyd

#endif  // CYD_NET_H
