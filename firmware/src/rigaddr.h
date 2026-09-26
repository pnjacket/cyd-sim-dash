// ENTITY-RIGADDRESS — the sim PC's hardware address, learned and persisted.
//
// The one thing this device knows that the operator never told it. That is deliberate and it follows
// the wire contract's reasoning: the plugin holds no configuration because the device announces
// itself, and the operator types no hardware address because the device can observe it.
//
// Persisted in its own NVS namespace rather than in ENTITY-DEVICECONFIG. That record is what the
// operator set; putting a learned value in it would show a field on the configuration page that
// nobody typed and nobody should edit, and would drag a learned fact through the schema versioning
// of a configured one.

#ifndef CYD_RIGADDR_H
#define CYD_RIGADDR_H

#include <stdint.h>

#include <IPAddress.h>

namespace cyd {
namespace rigaddr {

// Loads the stored address, if there is one. Call once at boot — after WiFi, because there is no
// point knowing the address before there is a network to send on.
void begin();

// True once an address has been learned, in this session or a previous one.
bool known();

// The learned address. Valid only when known(); six octets.
const uint8_t* mac();

// Monotonic ms at which it was last confirmed, in THIS session. Zero when the address came from
// storage and has not been re-confirmed since boot — which is the common case for the capability
// this exists to serve, since the rig being off is exactly why the panel is being touched.
uint32_t learnedAtMs();

// Looks `addr` up in the device's own ARP cache and stores what it finds. Call while the PC is known
// to be reachable — a frame has just been accepted from it, so the cache entry is fresh.
//
// Writes to flash only when the address has actually changed. Called on every accepted frame this
// would otherwise be a flash write at 60 Hz, which would wear out the part in a session.
//
// Returns true if an address is held afterwards, whether or not this call is what learned it.
bool observe(const IPAddress& addr);

}  // namespace rigaddr
}  // namespace cyd

#endif  // CYD_RIGADDR_H
