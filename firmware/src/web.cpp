#include "web.h"

#include <stdio.h>

namespace cyd {
namespace web {
namespace {

WebServer g_server(80);
bool      g_started = false;

// Every unknown path gets the same shape as every other error this product reports, rather than the
// framework's own 404 page. ERR-UNKNOWN-PATH exists so that a typo in a test URL fails in a way the
// test can recognise instead of returning HTML that happens not to parse.
void handleNotFound() {
  sendError(404, "unknownPath", "no such endpoint");
}

}  // namespace

WebServer& server() { return g_server; }

void sendError(int code, const char* error, const char* message) {
  char body[256];
  snprintf(body, sizeof(body),
           "{\"error\":\"%.32s\",\"message\":\"%.128s\"}",
           error == nullptr ? "error" : error,
           message == nullptr ? "" : message);
  g_server.send(code, "application/json", body);
}

void begin() {
  if (g_started) return;
  g_server.onNotFound(handleNotFound);
  g_server.begin();
  g_started = true;
}

void handle() {
  if (g_started) g_server.handleClient();
}

}  // namespace web
}  // namespace cyd
