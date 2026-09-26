#include "wake.h"

namespace cyd {
namespace wake {

size_t buildMagicPacket(const uint8_t mac[6], uint8_t* out, size_t cap) {
  if (out == nullptr || cap < kMagicPacketBytes) return 0;
  for (size_t i = 0; i < 6; ++i) out[i] = 0xFF;
  for (size_t rep = 0; rep < 16; ++rep) {
    for (size_t i = 0; i < 6; ++i) out[6 + rep * 6 + i] = mac[i];
  }
  return kMagicPacketBytes;
}

void Sequence::tick(uint32_t nowMs) {
  if (armed_ && (nowMs - armedAtMs_) >= kOfferWindowMs) armed_ = false;
}

TouchAction Sequence::onTouch(uint32_t nowMs, LinkState link, bool macKnown) {
  // The offer is lapsed here as well as in tick(), so a touch arriving after the window cannot be
  // read against a stale arming. Without this, a loop that skipped a tick would send on a touch the
  // operator meant as a first one.
  tick(nowMs);

  // The action lives in exactly one link state. Anywhere else a touch is only a light — including
  // `stale`, which looks similar from the seat but means the PC was answering a moment ago and does
  // not need waking.
  if (link != LinkState::Unreachable) {
    armed_ = false;
    return TouchAction::Lit;
  }

  // INV-WAKE-NEEDS-LEARNED-MAC. No address, no offer: the panel lights and says nothing further,
  // rather than inviting a second touch that would put no packet on the wire. An action that appears
  // to work and does nothing is worse than an action that is absent.
  if (!macKnown) {
    armed_ = false;
    return TouchAction::Lit;
  }

  if (armed_) {
    armed_ = false;              // one packet per sequence; a third touch starts over
    return TouchAction::Sent;
  }

  armed_ = true;
  armedAtMs_ = nowMs;
  return TouchAction::Offered;
}

}  // namespace wake
}  // namespace cyd
