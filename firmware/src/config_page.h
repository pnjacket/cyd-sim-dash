// SCREEN-CONFIG / UIF-CONFIG — the configuration page, served on the device's LAN address.
//
// This is what closes the dead-end the doc set predicted and the device then actually reached: with
// no sim-PC address stored, the link ladder reports `unresolved` forever, and before this page
// existed the only way out was to re-run provisioning. The panel shipped into exactly that state.
//
// Four fields, the same as the portal, pre-filled except the secrets. A blank secret field means
// LEAVE UNCHANGED, and the form says so — otherwise an operator changing only the host would
// silently wipe their WiFi passphrase.
//
// Gated by the device credential. The awkward case is a device whose stored credential is empty,
// which SEC-CREDENTIAL-POLICY forbids but which an earlier firmware allowed: see the note on
// recovery in the implementation.

#ifndef CYD_CONFIG_PAGE_H
#define CYD_CONFIG_PAGE_H

#include "config.h"
#include "net_counters.h"

namespace cyd {
namespace configpage {

/// Register the handlers on the shared server. Call before web::begin().
void begin(DeviceConfig& live);

/// Publish the counters the page displays.
void publishCounters(const net::Counters& counters);

/// True once a save has asked for a reboot, so the caller can honour it outside the handler.
bool rebootRequested();

/// Hand any staged WiFi change to the ESP32's WiFi store. Call immediately before restarting:
/// WiFi.begin() drops the current association, so it cannot run inside a request handler.
void applyPendingWifi();

}  // namespace configpage
}  // namespace cyd

#endif  // CYD_CONFIG_PAGE_H
