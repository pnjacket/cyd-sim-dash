#include "net.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

#include "wake.h"

namespace cyd {
namespace net {
namespace {

WiFiUDP  g_udp;
bool     g_listening = false;
uint32_t g_lastRegisterMs = 0;
uint32_t g_lastAcceptedMs = 0;
bool     g_everAccepted = false;

// Where the newest accepted frame came from.
//
// The sender's address rather than a resolution of pcHost, and the difference matters: this is the
// address that demonstrably answered, which is exactly the one the ARP cache will have an entry for.
// Resolving the configured name again would give the same answer on a good day and a stale or
// unrelated one otherwise, and ENTITY-RIGADDRESS would then be learned against the wrong machine.
IPAddress g_lastSender;
bool      g_haveSender = false;
Counters g_counters;

// The whole receive path's memory, allocated once. SEC-INPUT-BOUND: a fixed buffer and no dynamic
// allocation, so a flood of datagrams cannot fragment a heap the device has no way to defragment.
// One byte spare for the NUL the parser wants.
uint8_t g_buffer[kMaxDatagramBytes + 1];

// Maps a JSON string to the Spotter domain. An unknown string is Unavailable rather than None:
// claiming the road is clear on the strength of a value we do not understand is the one wrong
// answer, because it is indistinguishable from a real all-clear.
Spotter spotterFrom(JsonVariantConst v) {
  if (v.isNull()) return Spotter::Unavailable;
  const char* s = v.as<const char*>();
  if (s == nullptr) return Spotter::Unavailable;
  if (strcmp(s, "none") == 0) return Spotter::None;
  if (strcmp(s, "one") == 0)  return Spotter::One;
  if (strcmp(s, "two") == 0)  return Spotter::Two;
  return Spotter::Unavailable;
}

Status statusFrom(const char* s, bool& known) {
  known = true;
  if (s == nullptr)                          { known = false; return Status::NoSim; }
  if (strcmp(s, "live") == 0)                return Status::Live;
  if (strcmp(s, "noSim") == 0)               return Status::NoSim;
  if (strcmp(s, "unsupportedTitle") == 0)    return Status::UnsupportedTitle;
  if (strcmp(s, "adapterFault") == 0)        return Status::AdapterFault;
  known = false;
  return Status::NoSim;
}

// Parse one datagram into a Frame. Returns false for anything that is not recognisably a frame;
// the caller counts it as malformed. Validation of RANGES is not done here — that is validate()'s
// job, and keeping the two separate is what lets the unit tier exercise every range rule on the
// host without a network.
bool parseFrame(const uint8_t* data, size_t length, Frame& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, length);
  if (err) return false;
  if (!doc.is<JsonObject>()) return false;

  JsonObjectConst o = doc.as<JsonObjectConst>();

  // Version and status are structural: a frame without them is not a frame.
  if (!o["protocolMajor"].is<int>()) return false;
  if (!o["protocolMinor"].is<int>()) return false;
  out.protocolMajor = static_cast<uint16_t>(o["protocolMajor"].as<int>());
  out.protocolMinor = static_cast<uint16_t>(o["protocolMinor"].as<int>());

  bool statusKnown = false;
  out.status = statusFrom(o["status"].as<const char*>(), statusKnown);
  if (!statusKnown) return false;

  if (!o["stamp"].is<uint32_t>() && !o["stamp"].is<int>()) return false;
  out.stamp = o["stamp"].as<uint32_t>();

  // titleId: copied into a fixed field and always NUL-terminated. Truncation is preferable to
  // rejection, because this string exists to be shown to a human.
  memset(out.titleId, 0, sizeof(out.titleId));
  const char* title = o["titleId"].as<const char*>();
  if (title != nullptr) {
    strncpy(out.titleId, title, sizeof(out.titleId) - 1);
  }

  // Telemetry. Each element is independently present-or-unavailable, mirroring the producer's
  // per-element degradation: one missing element must not cost the others.
  memset(out.gear, 0, sizeof(out.gear));
  const char* gear = o["gear"].as<const char*>();
  out.gearPresent = gear != nullptr;
  if (out.gearPresent) strncpy(out.gear, gear, sizeof(out.gear) - 1);

  out.rpmPresent = o["rpm"].is<float>();
  out.rpm = out.rpmPresent ? o["rpm"].as<float>() : 0.0f;

  out.rampPresent = o["rampStartRpm"].is<float>();
  out.rampStartRpm = out.rampPresent ? o["rampStartRpm"].as<float>() : 0.0f;

  out.flashPresent = o["flashRpm"].is<float>();
  out.flashRpm = out.flashPresent ? o["flashRpm"].as<float>() : 0.0f;

  out.spotterLeft  = spotterFrom(o["spotterLeft"]);
  out.spotterRight = spotterFrom(o["spotterRight"]);

  return true;
}

}  // namespace

bool begin() {
  if (g_listening) return true;
  g_listening = g_udp.begin(kPort);
  return g_listening;
}

void maintainRegistration(const char* deviceId, const char* firmwareVersion,
                          const char* pcHost, uint32_t nowMs) {
  if (!g_listening) return;
  if (pcHost == nullptr || pcHost[0] == '\0') return;   // nothing to announce to, yet
  if (nowMs - g_lastRegisterMs < kRegisterIntervalMs && g_lastRegisterMs != 0) return;
  g_lastRegisterMs = nowMs;

  // Built with snprintf rather than a JSON document: the shape is fixed, it is the only thing this
  // device ever sends, and a stack buffer keeps the send path allocation-free like the receive one.
  char payload[160];
  const int n = snprintf(payload, sizeof(payload),
                         "{\"protocolMajor\":%u,\"protocolMinor\":%u,"
                         "\"deviceId\":\"%s\",\"firmwareVersion\":\"%s\"}",
                         static_cast<unsigned>(kProtocolMajor),
                         static_cast<unsigned>(kProtocolMinor),
                         deviceId == nullptr ? "" : deviceId,
                         firmwareVersion == nullptr ? "" : firmwareVersion);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(payload)) return;

  // Resolution happens here rather than being cached, so a DHCP change on the PC is picked up by
  // the next registration instead of stranding the device until it reboots.
  if (g_udp.beginPacket(pcHost, kPort)) {
    g_udp.write(reinterpret_cast<const uint8_t*>(payload), static_cast<size_t>(n));
    g_udp.endPacket();
  }
}

PollResult poll(StampTracker& stamps, Counters& counters, uint32_t nowMs, int maxDatagrams) {
  PollResult result;
  if (!g_listening) return result;

  for (int i = 0; i < maxDatagrams; ++i) {
    const int size = g_udp.parsePacket();
    if (size <= 0) break;

    // SEC-INPUT-BOUND. Oversized datagrams are drained and counted WITHOUT being parsed — the
    // parser never sees them, which is the point: the mitigation cannot depend on the parser
    // behaving well on hostile input.
    if (static_cast<size_t>(size) > kMaxDatagramBytes) {
      g_udp.flush();
      counters.oversized++;
      g_counters.oversized++;
      continue;
    }

    const int read = g_udp.read(g_buffer, kMaxDatagramBytes);
    if (read <= 0) continue;
    g_buffer[read] = '\0';

    Frame f;
    if (!parseFrame(g_buffer, static_cast<size_t>(read), f)) {
      counters.malformed++;
      g_counters.malformed++;
      continue;
    }

    const Reject verdict = acceptFrame(f, stamps);
    switch (verdict) {
      case Reject::Accepted:
        g_lastAcceptedMs = nowMs;
        g_everAccepted = true;
        g_lastSender = g_udp.remoteIP();
        g_haveSender = true;
        result.frameAccepted = true;
        result.frame = f;
        break;

      case Reject::VersionMajor:
        // Hard: its own screen, naming both versions. Counted and reported upward rather than
        // swallowed, because the user needs to be told which side to update.
        counters.versionRejected++;
        g_counters.versionRejected++;
        result.versionRejected = true;
        result.rejectedMajor = f.protocolMajor;
        result.rejectedMinor = f.protocolMinor;
        break;

      case Reject::FieldRange:
      case Reject::StatusInconsistent:
        counters.fieldRange++;
        g_counters.fieldRange++;
        break;

      case Reject::OutOfOrder:
        counters.outOfOrder++;
        g_counters.outOfOrder++;
        break;
    }
  }

  return result;
}

const Counters& counters() { return g_counters; }
uint32_t lastAcceptedAtMs() { return g_lastAcceptedMs; }
bool everAccepted() { return g_everAccepted; }

bool lastSender(IPAddress& out) {
  if (!g_haveSender) return false;
  out = g_lastSender;
  return true;
}

bool sendWake(const uint8_t mac[6]) {
  if (!g_listening || mac == nullptr) return false;

  uint8_t packet[wake::kMagicPacketBytes];
  if (wake::buildMagicPacket(mac, packet, sizeof(packet)) != sizeof(packet)) return false;

  // Broadcast to the local subnet, not to the rig's IP.
  //
  // This is not a shortcut. The machine is powered down, so nothing there will answer an ARP query
  // for its address; a unicast datagram would be dropped by this device's own stack before it ever
  // reached the wire. The magic packet carries the destination in its payload instead, which is the
  // whole reason the format exists, and the adapter recognises its own address in a frame addressed
  // to everyone.
  //
  // Port 9 by convention. Nothing listens on it — the packet is consumed by the network adapter
  // rather than by any software, which is also why there is no acknowledgement to wait for and
  // nothing here returns whether it worked. EVT-WAKE records that as a post-condition: none, and
  // none observable.
  if (!g_udp.beginPacket(WiFi.broadcastIP(), kWakePort)) return false;
  g_udp.write(packet, sizeof(packet));
  return g_udp.endPacket() == 1;
}

}  // namespace net
}  // namespace cyd
