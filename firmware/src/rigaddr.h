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

// Discards the learned address, in memory and in flash.
//
// Called when `pcHost` changes and when the configuration is erased. Both are cases where the stored
// address is about to become a lie: it was learned for one machine, and keeping it would aim a wake
// at whatever was at that address before. On a panel moved from a test machine to the real rig, that
// failure looks exactly like Wake-on-LAN being disabled in the rig's BIOS - a packet goes out, the
// rig ignores it, and four of the five likely causes are settings on another machine.
//
// The erase case is also a small privacy matter: a MAC is a machine on the previous owner's LAN, and
// an erase that leaves it behind is a gesture rather than a mechanism.
void forget();

// Looks the sim PC up in the device's own ARP cache and stores what it finds. Call when a frame has
// just been accepted, so the cache entry is fresh.
//
// `sender` is where that frame came from and `expected` is the resolved `pcHost`. **Learning happens
// only when they match.** Taking the sender alone was the earlier rule and it learned the wrong
// machine in practice, not just in theory: a bench tool feeding the panel from a third host taught it
// that host's address, and the panel then held a perfectly valid address for a machine nobody wanted
// woken. Nothing about that was visible - the address was known, the packet was well formed, and the
// rig ignored it, which is indistinguishable from Wake-on-LAN being switched off there.
//
// A non-matching sender is counted rather than ignored silently, because a host other than the
// configured one feeding this panel is worth knowing about either way.
//
// Writes to flash only when the address has actually changed. Called on every accepted frame, this
// would otherwise be a flash write at 60 Hz, which would wear the part out in a session.
//
// Returns true if an address is held afterwards, whether or not this call is what learned it.
bool observe(const IPAddress& sender, const IPAddress& expected);

// The learned address as lower-case colon-separated hex, or an empty string when none is held.
//
// Reported by API-STATE. `known()` answers "will a touch offer a wake"; this answers "would it wake
// the right machine", which is a different question and the one that cannot be answered any other way
// short of catching the packet on the wire.
const char* macText();

// How many accepted frames arrived from a host other than the configured one, since boot.
uint32_t foreignSenderCount();

}  // namespace rigaddr
}  // namespace cyd

#endif  // CYD_RIGADDR_H
