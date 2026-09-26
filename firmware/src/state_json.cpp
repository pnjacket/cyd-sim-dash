// The API-STATE projection, separated from the HTTP server that serves it.
//
// This file includes nothing from the Arduino networking stack, which is the entire point: it
// compiles on the host, so the unit tier can assert the exact JSON the device will serve without
// a device, a network, or a running server. state_api.cpp is then only plumbing.

#include "state_json.h"

#include <stdio.h>
#include <string.h>

namespace cyd {
namespace stateapi {
namespace {

const char* shiftName(ShiftPhase p) {
  switch (p) {
    case ShiftPhase::Neutral:  return "neutral";
    case ShiftPhase::Ramping:  return "ramping";
    case ShiftPhase::Flashing: return "flashing";
    case ShiftPhase::Unavailable:
    default:                   return "unavailable";
  }
}

// Driving is absent from this table on purpose: the ladder produces no link state while a fresh
// live frame is in hand, and the contract says it serialises to null rather than to a name.
const char* linkName(LinkState s) {
  switch (s) {
    case LinkState::DrivingPending:   return "drivingPending";
    case LinkState::Joining:          return "joining";
    case LinkState::Unresolved:       return "unresolved";
    case LinkState::Unreachable:      return "unreachable";
    case LinkState::Stale:            return "stale";
    case LinkState::NoSim:            return "noSim";
    case LinkState::UnsupportedTitle: return "unsupportedTitle";
    case LinkState::AdapterFault:     return "adapterFault";
    case LinkState::VersionMismatch:  return "versionMismatch";
    case LinkState::Driving:
    default:                          return nullptr;
  }
}

// JSON string escaping, for the two fields that carry text from outside: the configured host, typed
// by a human into the portal, and titleId, which arrives over the network. Neither is trusted.
size_t appendEscaped(char* out, size_t capacity, size_t at, const char* s) {
  if (s == nullptr) return at;
  for (const char* p = s; *p != '\0' && at + 2 < capacity; ++p) {
    const char c = *p;
    if (c == '"' || c == '\\') {
      out[at++] = '\\';
      out[at++] = c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      out[at++] = ' ';
    } else {
      out[at++] = c;
    }
  }
  return at;
}

}  // namespace

size_t render(char* out, size_t capacity,
              const DisplayState& state, const Context& context, const net::Counters& counters) {
  if (out == nullptr || capacity == 0) return 0;

  size_t at = 0;
  int n = snprintf(out, capacity, "{\"gearGlyph\":");
  at = (n > 0) ? static_cast<size_t>(n) : 0;

  if (state.gearPresent) {
    n = snprintf(out + at, capacity - at, "\"%.2s\"", state.gearGlyph);
  } else {
    n = snprintf(out + at, capacity - at, "null");
  }
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  n = snprintf(out + at, capacity - at, ",\"shiftPhase\":\"%s\",\"rampPosition\":",
               shiftName(state.shift));
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  // rampPosition is meaningful only while ramping; at any other phase it is null rather than a
  // stale number, so a consumer cannot mistake a leftover value for a live one.
  if (state.shift == ShiftPhase::Ramping) {
    n = snprintf(out + at, capacity - at, "%.4f", static_cast<double>(state.rampPosition));
  } else {
    n = snprintf(out + at, capacity - at, "null");
  }
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  n = snprintf(out + at, capacity - at, ",\"barLeft\":%s,\"barRight\":%s,\"linkState\":",
               state.barLeft ? "true" : "false",
               state.barRight ? "true" : "false");
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  const char* link = context.linkStateApplies ? linkName(context.linkState) : nullptr;
  if (link == nullptr) {
    n = snprintf(out + at, capacity - at, "null");
  } else {
    n = snprintf(out + at, capacity - at, "\"%s\"", link);
  }
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  n = snprintf(out + at, capacity - at, ",\"configuredHost\":\"");
  at += (n > 0) ? static_cast<size_t>(n) : 0;
  at = appendEscaped(out, capacity, at, context.configuredHost);
  n = snprintf(out + at, capacity - at, "\",\"lastFrameAgeMs\":");
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  if (context.lastFrameAgePresent) {
    n = snprintf(out + at, capacity - at, "%lu", static_cast<unsigned long>(context.lastFrameAgeMs));
  } else {
    n = snprintf(out + at, capacity - at, "null");
  }
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  // The four counters the contract names, plus oversized. All since boot, none persisted.
  // firmwareVersion and deviceId are additive: the contract allows fields to be added within a
  // firmware series, and these two are what make a test run self-identifying — without them a
  // failing E2E run cannot say which binary it was talking to.
  n = snprintf(out + at, capacity - at,
               ",\"malformedCount\":%lu,\"fieldRangeCount\":%lu,\"outOfOrderCount\":%lu"
               ",\"versionRejectedCount\":%lu,\"oversizedCount\":%lu"
               ",\"firmwareVersion\":\"%.15s\",\"deviceId\":\"%.15s\""
               ",\"lastDrawUs\":%lu,\"worstDrawUs\":%lu"
               ",\"uptimeMs\":%lu,\"resetReason\":\"%.24s\",\"drawCount\":%lu"
               ",\"backlightOn\":%s,\"blankAfterMinutes\":%u}",
               static_cast<unsigned long>(counters.malformed),
               static_cast<unsigned long>(counters.fieldRange),
               static_cast<unsigned long>(counters.outOfOrder),
               static_cast<unsigned long>(counters.versionRejected),
               static_cast<unsigned long>(counters.oversized),
               context.firmwareVersion == nullptr ? "" : context.firmwareVersion,
               context.deviceId == nullptr ? "" : context.deviceId,
               static_cast<unsigned long>(context.lastDrawUs),
               static_cast<unsigned long>(context.worstDrawUs),
               static_cast<unsigned long>(context.uptimeMs),
               context.resetReason == nullptr ? "" : context.resetReason,
               static_cast<unsigned long>(context.drawCount),
               context.backlightOn ? "true" : "false",
               static_cast<unsigned>(context.blankAfterMinutes));
  at += (n > 0) ? static_cast<size_t>(n) : 0;

  if (at >= capacity) at = capacity - 1;
  out[at] = '\0';
  return at;
}


}  // namespace stateapi
}  // namespace cyd
