// The soft-fault counters, split out from net.h so that the API-STATE projection can be compiled
// on the host.
//
// net.h pulls in the Arduino networking stack; these counters are a plain struct that the pure
// projection needs and nothing else. Separating them is what lets state_json.cpp - and therefore
// the unit tier - build without a device.

#ifndef CYD_NET_COUNTERS_H
#define CYD_NET_COUNTERS_H

#include <stdint.h>

namespace cyd {
namespace net {

/// Since boot, never persisted. Projected by API-STATE.
struct Counters {
  uint32_t malformed = 0;
  uint32_t fieldRange = 0;
  uint32_t outOfOrder = 0;
  uint32_t versionRejected = 0;
  uint32_t oversized = 0;   // SEC-INPUT-BOUND rejections, counted separately from malformed
};

}  // namespace net
}  // namespace cyd

#endif  // CYD_NET_COUNTERS_H
