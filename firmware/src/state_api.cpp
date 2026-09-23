#include "state_api.h"

#include "web.h"

#include <stdio.h>
#include <string.h>

namespace cyd {
namespace stateapi {
namespace {

bool g_registered = false;

// The published snapshot. Written by the render loop, read by the HTTP handler. Both run on the
// same core in this firmware, so no lock is needed — stated explicitly because it is the kind of
// assumption that silently stops being true when something is moved to the other core.
DisplayState   g_state;
Context        g_context;
net::Counters  g_counters;
char           g_hostBuf[64] = {0};
char           g_deviceBuf[16] = {0};
char           g_versionBuf[16] = {0};

void handleState() {
  char body[640];
  render(body, sizeof(body), g_state, g_context, g_counters);
  web::server().send(200, "application/json", body);
}

}  // namespace

void begin() {
  if (g_registered) return;
  web::server().on("/state", HTTP_GET, handleState);
  g_registered = true;
}

void publish(const DisplayState& state, const Context& context, const net::Counters& counters) {
  g_state = state;
  g_counters = counters;

  // The Context's pointers may be to caller stack memory, so the strings are copied rather than
  // aliased. An endpoint that serves a dangling pointer fails in a way that looks like anything
  // but the actual cause.
  strncpy(g_hostBuf, context.configuredHost == nullptr ? "" : context.configuredHost,
          sizeof(g_hostBuf) - 1);
  strncpy(g_deviceBuf, context.deviceId == nullptr ? "" : context.deviceId, sizeof(g_deviceBuf) - 1);
  strncpy(g_versionBuf, context.firmwareVersion == nullptr ? "" : context.firmwareVersion,
          sizeof(g_versionBuf) - 1);

  g_context = context;
  g_context.configuredHost = g_hostBuf;
  g_context.deviceId = g_deviceBuf;
  g_context.firmwareVersion = g_versionBuf;
}

}  // namespace stateapi
}  // namespace cyd
