// COMPONENT-WEB — the device's HTTP surface, on its LAN address.
//
// One server, two surfaces: API-STATE at /state and SCREEN-CONFIG at /. They were previously going
// to be two servers on one port, which cannot work; this module owns the socket and the other two
// register handlers against it.
//
// Both are on port 80 and neither is on the provisioning access point — that is SCREEN-PORTAL,
// which WiFiManager owns and which only exists before the device has joined a network.

#ifndef CYD_WEB_H
#define CYD_WEB_H

#include <WebServer.h>

namespace cyd {
namespace web {

/// The shared server. Handlers are registered against it before begin().
WebServer& server();

/// Start listening. Idempotent; safe to call after every handler is registered.
void begin();

/// Pump the server. Cheap; call every loop.
void handle();

/// The uniform JSON error rendering, used by every surface so that a failure is recognisable to a
/// test rather than arriving as whatever the framework would have produced.
void sendError(int code, const char* error, const char* message);

}  // namespace web
}  // namespace cyd

#endif  // CYD_WEB_H
