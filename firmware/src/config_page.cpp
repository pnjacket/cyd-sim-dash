#include "config_page.h"

#include "web.h"

#include <WiFi.h>
#include <stdio.h>
#include <string.h>

namespace cyd {
namespace configpage {
namespace {

DeviceConfig* g_live = nullptr;
net::Counters g_counters;
bool          g_rebootRequested = false;

// The device's WiFi credentials are held by WiFiManager in its own NVS namespace, not in
// DeviceConfig, so this page changes the two fields this product owns: the sim-PC address and the
// device credential. Changing the WiFi network means re-provisioning, which is honest — a device
// that cannot reach the new network could not serve this page to confirm the change anyway.

bool credentialIsSet() {
  return g_live != nullptr && g_live->credential[0] != '\0';
}

// Authentication.
//
// The awkward case: a device whose stored credential is EMPTY. SEC-CREDENTIAL-POLICY forbids that,
// but an earlier firmware accepted it at the portal and the panel shipped in exactly that state, so
// refusing to serve the page would leave the operator with no way out except re-provisioning — the
// dead-end this page exists to remove.
//
// So an unset credential serves the page, and the page then REQUIRES one to be set before anything
// can be saved. That repairs the violation rather than perpetuating it, and it is strictly better
// than the alternative of a permanently unauthenticated page.
bool authorised() {
  if (!credentialIsSet()) return true;   // recovery path; the form forces a credential to be set
  return web::server().authenticate("admin", g_live->credential);
}

bool requireAuth() {
  if (authorised()) return true;
  web::server().requestAuthentication();
  return false;
}

void appendEscapedHtml(char* out, size_t capacity, const char* s) {
  // The stored host is shown back in an HTML attribute. It was typed by a human and is not
  // trusted here: a quote in it would otherwise break out of the attribute.
  size_t at = strlen(out);
  for (const char* p = s; *p != '\0' && at + 8 < capacity; ++p) {
    switch (*p) {
      case '&':  strcpy(out + at, "&amp;");  at += 5; break;
      case '<':  strcpy(out + at, "&lt;");   at += 4; break;
      case '>':  strcpy(out + at, "&gt;");   at += 4; break;
      case '"':  strcpy(out + at, "&quot;"); at += 6; break;
      case '\'': strcpy(out + at, "&#39;");  at += 5; break;
      default:   out[at++] = *p;                      break;
    }
  }
  out[at] = '\0';
}

void sendPage(const char* notice, bool noticeIsError) {
  char host[256] = {0};
  if (g_live != nullptr) appendEscapedHtml(host, sizeof(host), g_live->pcHost);

  // Built in pieces rather than one snprintf: the whole page comfortably exceeds what a single
  // stack buffer should hold on a device with this much RAM to spare for everything else.
  String page;
  page.reserve(3000);
  page += F("<!doctype html><html><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>cyd-sim-dash</title><style>"
            "body{font-family:system-ui,sans-serif;max-width:34rem;margin:2rem auto;padding:0 1rem;"
            "background:#111;color:#eee}"
            "h1{font-size:1.3rem}label{display:block;margin:1rem 0 .25rem;font-weight:600}"
            "input{width:100%;padding:.5rem;font-size:1rem;background:#222;color:#eee;"
            "border:1px solid #444;border-radius:4px}"
            ".hint{font-size:.85rem;color:#aaa;margin-top:.25rem}"
            "button{margin-top:1.5rem;padding:.6rem 1.2rem;font-size:1rem;cursor:pointer;"
            "background:#2a6;color:#fff;border:0;border-radius:4px}"
            ".danger{background:#a33}"
            ".notice{padding:.75rem;border-radius:4px;margin:1rem 0}"
            ".ok{background:#243;border:1px solid #2a6}"
            ".err{background:#422;border:1px solid #a33}"
            "table{width:100%;margin-top:2rem;border-collapse:collapse;font-size:.9rem}"
            "td{padding:.3rem 0;border-bottom:1px solid #333}"
            "td:last-child{text-align:right;color:#aaa}"
            "</style></head><body><h1>cyd-sim-dash</h1>");

  if (notice != nullptr && notice[0] != '\0') {
    page += noticeIsError ? F("<div class=\"notice err\">") : F("<div class=\"notice ok\">");
    page += notice;
    page += F("</div>");
  }

  if (!credentialIsSet()) {
    page += F("<div class=\"notice err\">This device has no credential set, so this page is "
              "currently unprotected. Set one below before saving anything else.</div>");
  }

  page += F("<form method=\"POST\" action=\"/save\">"
            "<label for=\"host\">Sim PC address</label>"
            "<input id=\"host\" name=\"host\" value=\"");
  page += host;
  page += F("\" placeholder=\"192.168.1.50\">"
            "<div class=\"hint\">The IP address or name of the PC running SimHub.</div>"
            "<label for=\"cred\">Device credential</label>"
            "<input id=\"cred\" name=\"cred\" type=\"password\" placeholder=\"unchanged\">"
            "<div class=\"hint\">Gates this page and firmware updates. "
            "<strong>Leave blank to keep the current one.</strong></div>"
            "<button type=\"submit\">Save</button></form>");

  // The soft-fault counters, since boot. Shown here because this is the page an operator opens
  // when something is wrong, and these four numbers are the first question anyone would ask.
  char counters[512];
  snprintf(counters, sizeof(counters),
           "<table>"
           "<tr><td>Malformed datagrams</td><td>%lu</td></tr>"
           "<tr><td>Field out of range</td><td>%lu</td></tr>"
           "<tr><td>Out of order</td><td>%lu</td></tr>"
           "<tr><td>Version rejected</td><td>%lu</td></tr>"
           "<tr><td>Oversized, dropped unparsed</td><td>%lu</td></tr>"
           "</table><div class=\"hint\">Counted since boot; never stored.</div>",
           static_cast<unsigned long>(g_counters.malformed),
           static_cast<unsigned long>(g_counters.fieldRange),
           static_cast<unsigned long>(g_counters.outOfOrder),
           static_cast<unsigned long>(g_counters.versionRejected),
           static_cast<unsigned long>(g_counters.oversized));
  page += counters;

  page += F("<form method=\"POST\" action=\"/erase\" "
            "onsubmit=\"return confirm('Erase all stored settings? The device will restart and "
            "raise its setup network.')\">"
            "<button class=\"danger\" type=\"submit\">Erase settings</button></form>"
            "</body></html>");

  web::server().send(200, "text/html", page);
}

void handleRoot() {
  if (!requireAuth()) return;
  sendPage("", false);
}

// ERR-PORTAL-INPUT: empty is always a validation failure, never a default. The one labelled
// exception is a blank SECRET, which means leave unchanged - and that is why the two fields are
// validated by different rules rather than by one shared "not empty" test.
bool hostLooksValid(const char* h) {
  const size_t n = strlen(h);
  if (n == 0 || n > 253) return false;
  for (const char* p = h; *p != '\0'; ++p) {
    const char c = *p;
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

void handleSave() {
  if (!requireAuth()) return;
  if (g_live == nullptr) { web::sendError(500, "noConfig", "configuration unavailable"); return; }

  WebServer& s = web::server();
  String host = s.hasArg("host") ? s.arg("host") : String();
  String cred = s.hasArg("cred") ? s.arg("cred") : String();

  host.trim();

  if (!hostLooksValid(host.c_str())) {
    sendPage("The sim PC address is required, and may contain only letters, digits, dots, "
             "hyphens and underscores.", true);
    return;
  }

  // The recovery case again: a device with no credential must set one here, because a blank field
  // means "unchanged" and unchanged would mean staying unprotected.
  if (!credentialIsSet() && cred.length() == 0) {
    sendPage("This device has no credential. Set one now - a blank field means "
             "&lsquo;unchanged&rsquo;, which would leave the page unprotected.", true);
    return;
  }

  strncpy(g_live->pcHost, host.c_str(), sizeof(g_live->pcHost) - 1);
  g_live->pcHost[sizeof(g_live->pcHost) - 1] = '\0';

  // A blank secret leaves the stored one alone. SEC-CREDENTIAL-POLICY imposes no minimum length
  // and no composition rule, so a single character is accepted deliberately.
  if (cred.length() > 0) {
    strncpy(g_live->credential, cred.c_str(), sizeof(g_live->credential) - 1);
    g_live->credential[sizeof(g_live->credential) - 1] = '\0';
  }

  if (!config::save(*g_live)) {
    sendPage("The settings could not be written to flash.", true);
    return;
  }

  sendPage("Saved. The device is restarting so the new address takes effect.", false);
  g_rebootRequested = true;
}

void handleErase() {
  if (!requireAuth()) return;

  // SEC-ERASE-OVERWRITE: config::erase() overwrites the stored values before deleting their keys,
  // so the secrets are not merely unreferenced but gone. Check S7 dumps flash to prove it, because
  // an erase that leaves recoverable plaintext is a gesture rather than a mechanism.
  config::erase();
  web::server().send(200, "text/html",
                     F("<!doctype html><html><body style=\"font-family:system-ui;background:#111;"
                       "color:#eee;max-width:34rem;margin:2rem auto;padding:0 1rem\">"
                       "<h1>Erased</h1><p>The device is restarting and will raise its setup "
                       "network.</p></body></html>"));
  g_rebootRequested = true;
}

}  // namespace

void begin(DeviceConfig& live) {
  g_live = &live;
  WebServer& s = web::server();
  s.on("/", HTTP_GET, handleRoot);
  s.on("/save", HTTP_POST, handleSave);
  s.on("/erase", HTTP_POST, handleErase);
}

void publishCounters(const net::Counters& counters) { g_counters = counters; }

bool rebootRequested() { return g_rebootRequested; }

}  // namespace configpage
}  // namespace cyd
