// cyd-sim-dash — ESP32-2432S028R firmware
//
// Slices 8 + 9 + part of 16, deliberately combined.
//
// Why combined: this board's auto-reset does not drive IO0, so every USB flash needs a manual
// unplug / hold BOOT / replug sequence. Getting WiFi and OTA in on the *first* flash makes it the
// last one — everything after this, including fixing the display driver if it is still wrong, goes
// over the air. The reordering is recorded in IMPLEMENTATION.md rather than done quietly.
//
// What runs here:
//   - panel bring-up and SCREEN-BOOT
//   - configuration load from NVS; captive portal when unprovisioned or unable to join
//   - WiFi association, host resolution, and OTA updates gated by the device credential
//   - the real linkState ladder driving the panel from real inputs
//
// Slice 10 adds: the UDP receive path, registration, and API-STATE.
//
// API-STATE matters beyond the contract it realises. Once the panel is off the USB cable there is
// no serial, and every diagnostic this firmware prints becomes unreadable. `curl <device>/state`
// replaces all of it, and works from the machine running the tests rather than only from the one
// holding the cable.
//
// Not yet: the driving screen and the link icons (slices 11-14).

#include <ArduinoOTA.h>
#include <esp_system.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "src/config.h"
#include "src/config_page.h"
#include "src/display_state.h"
#include "src/identity.h"
#include "src/net.h"
#include "src/panel.h"
#include "src/rigaddr.h"
#include "src/touch.h"
#include "src/wake.h"
#include "src/state_api.h"
#include "src/web.h"

using namespace cyd;

static constexpr uint32_t kBootScreenMs = 3000;

static char          g_deviceId[13] = {0};
static DeviceConfig  g_cfg;
static bool          g_hostResolved = false;
static uint32_t      g_connectedAtMs = 0;
static LinkState     g_shown = static_cast<LinkState>(0xFF);  // nothing drawn yet
static bool          g_otaRunning = false;

// True while an update is actually transferring.
//
// An OTA upload is a TCP stream that has to be serviced promptly, and it competes with everything
// else this loop does. Once the panel is fed by a real SimHub at ~60 Hz, the receive path, the
// renderer and the web server between them starve it enough that the upload aborts mid-transfer -
// observed on 2026-09-23 as "connection aborted by the software in your host machine", after the
// same push had worked repeatedly against an idle panel.
//
// The fix is to stop competing: while an update is in flight the loop does nothing but service it.
// Telemetry is dropped for those few seconds, which costs nothing - the device is about to reboot
// into new firmware anyway, and a panel mid-update is not one anybody is driving by.
static bool          g_otaInProgress = false;
static StampTracker  g_stamps;
static net::Counters g_counters;
static Frame         g_lastFrame;
static bool          g_haveFrame = false;
static bool          g_versionRejected = false;
static uint32_t      g_lastAcceptedMs = 0;

// CAP-BLANK. When the driving screen was last showing, and whether the glass is currently lit.
// Initialised at boot so a panel that starts with the rig off blanks after the configured period
// rather than staying lit forever waiting for a first frame it may never get.
static uint32_t      g_lastDrivingMs = 0;
static bool          g_backlightOn = true;

// CAP-WAKE-RIG. The two-touch sequence, and whether the offer is currently on the glass.
//
// g_offerShown is tracked separately from the sequence's own armed flag because the offer is a screen
// and g_shown - which decides whether the link screen needs redrawing - has no LinkState that means
// "offering". Conflating them would leave the offer on screen after the window lapsed.
static wake::Sequence g_wake;
static bool           g_offerShown = false;

// ---------------------------------------------------------------------------

// Why the device last booted, in words.
//
// This is what separates "the update restarted it" from "it crashed" or "the power sagged". The
// brownout case is not hypothetical on this hardware: a CYD drawing peak current over a marginal
// USB lead can dip below the supervisor threshold and reset, which from the outside is
// indistinguishable from a spontaneous reboot.
static const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "powerOn";
    case ESP_RST_SW:       return "software";      // an update or a saved configuration
    case ESP_RST_PANIC:    return "panic";         // a crash
    case ESP_RST_INT_WDT:  return "interruptWatchdog";
    case ESP_RST_TASK_WDT: return "taskWatchdog";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";      // the power supply, not the firmware
    case ESP_RST_DEEPSLEEP: return "deepSleep";
    case ESP_RST_EXT:      return "externalPin";
    case ESP_RST_SDIO:     return "sdio";
    case ESP_RST_UNKNOWN:
    default:               return "unknown";
  }
}

static void onPortalRaised(WiFiManager* wm) {
  Serial.print("[portal] access point up: ");
  Serial.println(wm->getConfigPortalSSID());
  Serial.println("[portal] join it, then set the sim PC address and a device credential");
  panel::drawPortal(wm->getConfigPortalSSID().c_str());
}

// Resolves the configured host. An empty host is not an error state of its own — the ladder has no
// "unconfigured" condition — so it reports as unresolved, which is what it honestly is.
static bool resolveHost() {
  if (g_cfg.pcHost[0] == '\0') return false;
  IPAddress addr;
  if (addr.fromString(g_cfg.pcHost)) return true;      // a literal address needs no lookup
  return WiFi.hostByName(g_cfg.pcHost, addr) == 1;
}

static void startOta() {
  ArduinoOTA.setHostname("cyd-sim-dash");
  if (g_cfg.credential[0] != '\0') {
    ArduinoOTA.setPassword(g_cfg.credential);   // SEC-CREDENTIAL-POLICY: non-empty, no minimum
  }
  ArduinoOTA
      .onStart([]() {
        g_otaInProgress = true;          // the loop yields everything else from here
        Serial.println("[ota] update starting - telemetry paused until it finishes");
        panel::drawMessage("Updating...");
      })
      .onEnd([]() { Serial.println("[ota] update complete, rebooting"); })
      .onProgress([](unsigned int done, unsigned int total) {
        Serial.printf("[ota] %u%%\r", total ? (done * 100u / total) : 0u);
      })
      .onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", e); });
  ArduinoOTA.begin();
  g_otaRunning = true;
  Serial.print("[ota] listening on ");
  Serial.println(WiFi.localIP());
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);

  deviceId(g_deviceId, sizeof(g_deviceId));

  Serial.println();
  Serial.println("cyd-sim-dash");
  Serial.print("  device   "); Serial.println(g_deviceId);
  Serial.print("  firmware "); Serial.println(kFirmwareVersion);
  Serial.print("  reset    "); Serial.println(resetReasonText());
  Serial.print("  protocol "); Serial.print(kProtocolMajor);
  Serial.print("."); Serial.println(kProtocolMinor);

  panel::begin();
  touch::begin();

  panel::drawBoot(g_deviceId, kFirmwareVersion);
  Serial.println("[boot] identity screen shown");
  delay(kBootScreenMs);

  const bool haveConfig = config::load(g_cfg);
  Serial.print("[config] ");
  Serial.println(haveConfig ? "loaded" : "unprovisioned or unreadable - portal will raise");

  // The portal collects the two fields WiFiManager does not own itself. Pre-filled from whatever
  // was stored, so a reconfiguration does not mean retyping everything.
  WiFiManager wm;
  WiFiManagerParameter pHost("host", "Sim PC address (IP or name)", g_cfg.pcHost, sizeof(g_cfg.pcHost) - 1);
  WiFiManagerParameter pCred("cred", "Device credential (OTA + config page)", g_cfg.credential, sizeof(g_cfg.credential) - 1);
  wm.addParameter(&pHost);
  wm.addParameter(&pCred);
  wm.setAPCallback(onPortalRaised);
  wm.setConfigPortalTimeout(0);         // stay up until configured; a blank panel helps nobody

  char apName[32];
  snprintf(apName, sizeof(apName), "cyd-sim-dash-%s", g_deviceId + 6);

  if (!wm.autoConnect(apName)) {
    Serial.println("[wifi] portal exited without a connection - restarting");
    ESP.restart();
  }

  strncpy(g_cfg.pcHost, pHost.getValue(), sizeof(g_cfg.pcHost) - 1);
  strncpy(g_cfg.credential, pCred.getValue(), sizeof(g_cfg.credential) - 1);
  config::save(g_cfg);

  g_connectedAtMs = millis();
  g_lastDrivingMs = g_connectedAtMs;
  Serial.print("[wifi] connected, ip ");
  Serial.println(WiFi.localIP());
  Serial.print("[config] pcHost '");
  Serial.print(g_cfg.pcHost);
  Serial.print("', credential ");
  Serial.println(g_cfg.credential[0] ? "set" : "EMPTY - OTA will be unauthenticated");

  g_hostResolved = resolveHost();
  Serial.print("[host] ");
  Serial.println(g_hostResolved ? "resolved" : "not resolved");

  startOta();

  // After WiFi, because a stored hardware address is only useful once there is a network to send on.
  rigaddr::begin();
  Serial.print("[wake] rig address ");
  Serial.println(rigaddr::known() ? "known from storage" : "not learned yet - wake unavailable");

  if (net::begin()) {
    Serial.print("[net] listening on udp/");
    Serial.println(net::kPort);
  } else {
    Serial.println("[net] could not bind the socket - frames will not arrive");
  }

  // One server, both surfaces. Registered before begin(), because handlers added afterwards are
  // not picked up.
  stateapi::begin();
  configpage::begin(g_cfg);
  web::begin();
  Serial.print("[web] http://");
  Serial.print(WiFi.localIP());
  Serial.println("/  (config)  and  /state  (diagnostics)");
}

void loop() {
  if (g_otaRunning) ArduinoOTA.handle();

  // While an update is transferring, nothing else runs. See g_otaInProgress.
  if (g_otaInProgress) return;

  web::handle();

  // A save or erase asks for a restart. Honoured out here rather than inside the handler, so the
  // response is actually delivered before the device goes away.
  if (configpage::rebootRequested()) {
    Serial.println("[config] restarting to apply changes");
    delay(400);                      // let the reply finish leaving before anything disturbs WiFi
    configpage::applyPendingWifi();
    delay(200);
    ESP.restart();
  }

  const uint32_t now = millis();

  net::maintainRegistration(g_deviceId, kFirmwareVersion, g_cfg.pcHost, now);

  const net::PollResult got = net::poll(g_stamps, g_counters, now);
  if (got.frameAccepted) {
    g_lastFrame = got.frame;
    g_haveFrame = true;
    g_lastAcceptedMs = now;
    g_versionRejected = false;   // a good frame clears a previous mismatch

    // ENTITY-RIGADDRESS is learned here and nowhere else: a frame has just arrived from the PC, so
    // the ARP cache has a fresh entry for it and the address is confirmed rather than inferred. This
    // is also the only moment it can be learned - by the time the operator wants a wake, the machine
    // is off and unfindable.
    IPAddress sender;
    if (net::lastSender(sender)) rigaddr::observe(sender);
  }
  if (got.versionRejected) g_versionRejected = true;

  // The real ladder on real inputs. Link-layer facts outrank frame contents, and freshness
  // outranks a stale frame's status - both are the ladder's business, not this loop's.
  LinkInputs in;
  in.wifiAssociated  = WiFi.status() == WL_CONNECTED;
  in.hostResolved    = g_hostResolved;
  in.everAccepted    = net::everAccepted();
  in.versionRejected = g_versionRejected;
  in.msSinceRegister = now - g_connectedAtMs;
  in.msSinceAccepted = net::everAccepted() ? (now - g_lastAcceptedMs) : 0;

  const Status frameStatus = g_haveFrame ? g_lastFrame.status : Status::NoSim;
  const LinkState state = deriveLink(in, frameStatus);

  // The full display state, derived by the engine from the newest frame and the same link inputs.
  // derive() reads the frame only when the ladder yields Driving, so a stale or faulted frame
  // cannot leak values onto the panel - that rule lives in the engine rather than here.
  //
  // Slices 11-14 render this; for now it is published to API-STATE, which is what makes it
  // assertable at all before the glass shows it.
  const DisplayState display = derive(g_lastFrame, in);

  // CAP-WAKE-RIG, before the backlight decision and before the screen, because a touch changes both.
  //
  // The offer is lapsed first so a touch arriving just after the window is read as a first touch
  // rather than a second. Then the press itself: every outcome lights the panel and restarts the
  // blanking period, which is what makes a dark panel usable and is the one way the backlight comes
  // on other than a live frame.
  g_wake.tick(now);
  if (g_offerShown && !g_wake.armed()) {
    g_offerShown = false;
    g_shown = static_cast<LinkState>(0xFF);   // the offer lapsed; the link screen is owed a redraw
  }

  bool wakeSent = false;
  if (touch::pressed(now)) {
    g_lastDrivingMs = now;                    // a touch restarts the blanking period
    const wake::TouchAction action = g_wake.onTouch(now, state, rigaddr::known());

    switch (action) {
      case wake::TouchAction::Offered:
        Serial.println("[wake] offer shown - a second touch sends the packet");
        panel::drawWakeOffer();
        g_offerShown = true;
        break;

      case wake::TouchAction::Sent:
        // EVT-WAKE has no observable post-condition, so this log line and the brief confirmation are
        // the only evidence the operator or a later investigation will ever have that it happened.
        wakeSent = net::sendWake(rigaddr::mac());
        Serial.print("[wake] magic packet ");
        Serial.println(wakeSent ? "sent" : "NOT sent - the send path failed");
        g_offerShown = false;
        g_shown = static_cast<LinkState>(0xFF);
        break;

      case wake::TouchAction::Lit:
        // Either the state has no action, or no address has ever been learned. The panel lights and
        // offers nothing - INV-WAKE-NEEDS-LEARNED-MAC. Saying so on serial is what makes the second
        // case diagnosable at all, since the glass looks identical either way.
        if (state == LinkState::Unreachable && !rigaddr::known()) {
          Serial.println("[wake] touched, but no rig address has been learned - nothing to offer");
        }
        break;
    }
  }

  stateapi::Context ctx;
  ctx.deviceId = g_deviceId;
  ctx.firmwareVersion = kFirmwareVersion;
  ctx.configuredHost = g_cfg.pcHost;
  ctx.linkStateApplies = state != LinkState::Driving;
  ctx.linkState = state;
  ctx.lastFrameAgePresent = net::everAccepted();
  ctx.lastFrameAgeMs = net::everAccepted() ? (now - g_lastAcceptedMs) : 0;
  ctx.lastDrawUs = panel::lastDrawUs();
  ctx.worstDrawUs = panel::worstDrawUs();
  ctx.uptimeMs = now;
  ctx.resetReason = resetReasonText();
  ctx.drawCount = panel::drawCount();
  ctx.backlightOn = g_backlightOn;
  ctx.blankAfterMinutes = g_cfg.blankAfterMinutes;
  ctx.wakeArmed = g_wake.armed();
  ctx.rigMacKnown = rigaddr::known();
  stateapi::publish(display, ctx, g_counters);
  configpage::publishCounters(g_counters);

  // Screen selection, and it is exactly the ladder's own answer rather than a second judgement:
  // SCREEN-DRIVING is showing precisely when deriveLink yields Driving, which happens only when a
  // fresh live frame is in hand. Anything else is SCREEN-LINK. Having one rule decide both the
  // link icon and which screen shows is what keeps them from ever disagreeing.
  // CAP-BLANK. The engine decides; this only drives the pin and remembers the transition, so the
  // rule stays testable on the host and the loop holds no policy.
  if (state == LinkState::Driving) g_lastDrivingMs = now;
  const bool wantLit = backlightShouldBeOn(state, g_otaInProgress,
                                           now - g_lastDrivingMs, g_cfg.blankAfterMinutes);
  if (wantLit != g_backlightOn) {
    panel::backlight(wantLit);
    g_backlightOn = wantLit;
    Serial.print("[panel] backlight ");
    Serial.println(wantLit ? "on" : "off");

    // Coming back from dark, nothing on the glass can be trusted to match the state - the scene was
    // drawn before the panel went out and the link condition may have changed since.
    if (wantLit) { panel::invalidate(); g_shown = static_cast<LinkState>(0xFF); }
  }

  // The confirmation after a wake. Held briefly and blocking, which is affordable precisely here: the
  // rig is off, so there is no telemetry to miss, and the alternative is a timer in the loop for a
  // message shown once per power-up.
  //
  // It does NOT say "waking". The panel has no way to know whether the rig is coming up, and a
  // message it cannot retract would keep asserting something it does not know. It reports what this
  // device actually did - it sent a packet - and then gets out of the way and lets the ladder speak.
  if (wakeSent) {
    panel::drawMessage("Wake sent");
    delay(1500);
    panel::invalidate();
  }

  if (state == LinkState::Driving) {
    if (g_shown != LinkState::Driving) {
      panel::invalidate();   // coming from the link screen: the glass holds something else
      Serial.println("[screen] driving");
    }
    panel::drawDriving(display, now);
  } else if (g_offerShown && state == LinkState::Unreachable) {
    // The offer owns the glass while it stands. Nothing to redraw - it is static - and redrawing the
    // link screen underneath it is exactly the bug this branch exists to prevent.
  } else if (state != g_shown) {
    panel::invalidate();
    panel::drawLink(state);
    Serial.print("[link] ");
    Serial.println(panel::linkLine(state));
  }
  g_shown = state;

  delay(10);   // tighter than before: the receive path and the HTTP server both want servicing
}
