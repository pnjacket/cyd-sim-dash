// API-STATE — GET /state, a read-only JSON projection of ENTITY-DISPLAYSTATE.
//
// Present in EVERY build including release, deliberately. An endpoint compiled only into test
// builds would mean the end-to-end tests prove a binary that is never shipped, which is the kind
// of gap that only shows up once.
//
// Unauthenticated, on the same argument as the UDP stream: it exposes gear, RPM and proximity —
// display data with no confidentiality value — and requiring a credential would mean the tests
// carry one, which is worse. It is read-only with no side effects of any kind.
//
// It also answers the practical problem the operator raised: once the panel is off the USB cable,
// serial is gone and there is no way to read anything from it. `curl http://<device>/state` replaces
// every Serial.print this project would otherwise have relied on — and unlike serial, it works from
// the machine running the tests rather than only from the one holding the cable.

#ifndef CYD_STATE_API_H
#define CYD_STATE_API_H

#include "display_state.h"
#include "net.h"
#include "state_json.h"   // Context and render() live here, host-compilable

namespace cyd {
namespace stateapi {

/// Register the /state handler on the shared server. Call before web::begin().
void begin();

/// Publish the state the endpoint should report. Called whenever the display state changes.
void publish(const DisplayState& state, const Context& context, const net::Counters& counters);

}  // namespace stateapi
}  // namespace cyd

#endif  // CYD_STATE_API_H
