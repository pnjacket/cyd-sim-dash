// Unit tier for the API-STATE projection.
//
// This is the endpoint the end-to-end tests read, and the one that replaces serial once the panel
// is off the USB cable. If its JSON is wrong, every test that consumes it is wrong in the same
// direction and nothing notices — so the projection is asserted directly, against the field list
// the contract specifies, rather than trusted because it looked right in a browser once.
//
// It compiles on the host because state_json.cpp includes no Arduino networking headers. That
// separation exists for exactly this reason.

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../src/state_json.h"

using namespace cyd;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, what)                                                        \
  do {                                                                           \
    ++g_checks;                                                                   \
    if (!(cond)) { ++g_failures; printf("  FAIL  %s\n", (what)); }                \
  } while (0)

static void section(const char* name) { printf("\n%s\n", name); }

static bool contains(const char* haystack, const char* needle) {
  return strstr(haystack, needle) != nullptr;
}

// ---------------------------------------------------------------------------

static void test_every_contracted_field_is_present() {
  section("API-STATE carries every field the contract names, always");

  DisplayState s;
  stateapi::Context c;
  net::Counters n;

  char body[stateapi::kStateBufferBytes];
  stateapi::render(body, sizeof(body), s, c, n);

  // The contract lists these by name. A field that vanishes when its value is empty is the classic
  // way a consumer starts guessing, so presence is asserted unconditionally.
  const char* required[] = {
    "\"gearGlyph\"", "\"shiftPhase\"", "\"rampPosition\"", "\"barLeft\"", "\"barRight\"",
    "\"linkState\"", "\"configuredHost\"", "\"lastFrameAgeMs\"",
    "\"malformedCount\"", "\"fieldRangeCount\"", "\"outOfOrderCount\"", "\"versionRejectedCount\"",
    // CAP-BLANK and CAP-WAKE-RIG both fail in ways that are invisible from the driving seat - a dark
    // panel looks dead, and a wake that will never be offered looks like a rig ignoring the packet.
    // These four fields are the whole of how those are told apart, so their presence is not optional.
    "\"backlightOn\"", "\"blankAfterMinutes\"", "\"wakeArmed\"", "\"rigMacKnown\"",
  };
  for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
    CHECK(contains(body, required[i]), required[i]);
  }

  CHECK(body[0] == '{', "the body is a JSON object");
  CHECK(body[strlen(body) - 1] == '}', "the body is closed");
}

static void test_unavailable_is_null_not_absent() {
  section("unavailable renders as null, never as a substituted value");

  DisplayState s;
  s.gearPresent = false;
  s.shift = ShiftPhase::Unavailable;

  stateapi::Context c;
  c.lastFrameAgePresent = false;

  net::Counters n;
  char body[stateapi::kStateBufferBytes];
  stateapi::render(body, sizeof(body), s, c, n);

  CHECK(contains(body, "\"gearGlyph\":null"), "an absent gear is null, not an empty string");
  CHECK(contains(body, "\"lastFrameAgeMs\":null"), "no frame yet means null, not 0");
  CHECK(contains(body, "\"shiftPhase\":\"unavailable\""), "an unavailable shift says so");

  // rampPosition is meaningful only while ramping. A leftover number at any other phase would be
  // indistinguishable from a live one to anything reading this endpoint.
  CHECK(contains(body, "\"rampPosition\":null"), "rampPosition is null unless ramping");
}

static void test_link_state_is_null_while_driving() {
  section("linkState is null while the driving screen is showing");

  DisplayState s;
  stateapi::Context c;
  net::Counters n;
  char body[stateapi::kStateBufferBytes];

  // The ladder produces no link state when a fresh live frame is in hand. The contract says that
  // serialises to null rather than to the name "driving" - there is no such link condition.
  c.linkStateApplies = false;
  c.linkState = LinkState::Driving;
  stateapi::render(body, sizeof(body), s, c, n);
  CHECK(contains(body, "\"linkState\":null"), "driving yields a null linkState");
  CHECK(!contains(body, "\"driving\""), "the string 'driving' never appears as a link state");

  c.linkStateApplies = true;
  c.linkState = LinkState::Stale;
  stateapi::render(body, sizeof(body), s, c, n);
  CHECK(contains(body, "\"linkState\":\"stale\""), "a real link condition is named");

  c.linkState = LinkState::VersionMismatch;
  stateapi::render(body, sizeof(body), s, c, n);
  CHECK(contains(body, "\"linkState\":\"versionMismatch\""), "versionMismatch is named in camelCase");
}

static void test_counters_are_projected() {
  section("the soft-fault counters are projected, since boot");

  DisplayState s;
  stateapi::Context c;
  net::Counters n;
  n.malformed = 3;
  n.fieldRange = 5;
  n.outOfOrder = 7;
  n.versionRejected = 11;
  n.oversized = 13;

  char body[stateapi::kStateBufferBytes];
  stateapi::render(body, sizeof(body), s, c, n);

  CHECK(contains(body, "\"malformedCount\":3"), "malformed count is projected");
  CHECK(contains(body, "\"fieldRangeCount\":5"), "field-range count is projected");
  CHECK(contains(body, "\"outOfOrderCount\":7"), "out-of-order count is projected");
  CHECK(contains(body, "\"versionRejectedCount\":11"), "version-rejected count is projected");
  CHECK(contains(body, "\"oversizedCount\":13"), "oversized count is projected");
}

static void test_values_render() {
  section("present values render as values");

  DisplayState s;
  s.gearPresent = true;
  strcpy(s.gearGlyph, "4");
  s.shift = ShiftPhase::Ramping;
  s.rampPosition = 0.5f;
  s.barLeft = true;
  s.barRight = false;

  stateapi::Context c;
  c.lastFrameAgePresent = true;
  c.lastFrameAgeMs = 123;
  c.configuredHost = "192.168.1.50";
  c.deviceId = "a1b2c3d4e5f6";
  c.firmwareVersion = "0.2.0";

  net::Counters n;
  char body[stateapi::kStateBufferBytes];
  stateapi::render(body, sizeof(body), s, c, n);

  CHECK(contains(body, "\"gearGlyph\":\"4\""), "a gear glyph renders");
  CHECK(contains(body, "\"shiftPhase\":\"ramping\""), "the ramping phase renders");
  CHECK(contains(body, "\"rampPosition\":0.5"), "the ramp position renders while ramping");
  CHECK(contains(body, "\"barLeft\":true"), "a lit left bar renders true");
  CHECK(contains(body, "\"barRight\":false"), "a dark right bar renders false");
  CHECK(contains(body, "\"lastFrameAgeMs\":123"), "the frame age renders");
  CHECK(contains(body, "\"configuredHost\":\"192.168.1.50\""), "the configured host renders");

  // Additive fields. They make a test run self-identifying: without them a failing end-to-end run
  // cannot say which binary it was talking to.
  CHECK(contains(body, "\"firmwareVersion\":\"0.2.0\""), "the firmware version is reported");
  CHECK(contains(body, "\"deviceId\":\"a1b2c3d4e5f6\""), "the device identity is reported");
}

static void test_wake_fields_render_both_ways() {
  section("CAP-WAKE-RIG's two fields distinguish the silent failure from the loud one");

  DisplayState s;
  net::Counters n;
  char body[stateapi::kStateBufferBytes];

  // The case that matters: an address has never been learned, so no touch will ever offer a wake.
  // Nothing on the glass says so - by design, since offering an action that would do nothing is worse
  // - which leaves this endpoint as the only way to tell it from a rig that ignores the packet.
  stateapi::Context none;
  none.rigMacKnown = false;
  none.wakeArmed = false;
  stateapi::render(body, sizeof(body), s, none, n);
  CHECK(contains(body, "\"rigMacKnown\":false"), "an unlearned address reports false");
  CHECK(contains(body, "\"wakeArmed\":false"), "and nothing is armed");

  stateapi::Context armed;
  armed.rigMacKnown = true;
  armed.wakeArmed = true;
  stateapi::render(body, sizeof(body), s, armed, n);
  CHECK(contains(body, "\"rigMacKnown\":true"), "a learned address reports true");
  CHECK(contains(body, "\"wakeArmed\":true"), "a standing offer reports armed");

  // SEC-WAKE-PHYSICAL-ONLY. The endpoint reports whether the action is armed and offers no way to
  // fire it. Asserted here because the temptation to add a POST for testing is exactly how a
  // physical-only control acquires a network path.
  CHECK(!contains(body, "wakeNow"), "no field invites triggering a wake");
  CHECK(!contains(body, "sendWake"), "and none names the send path");
}

static void test_hostile_strings_cannot_break_the_json() {
  section("text from outside cannot break the document");

  DisplayState s;
  stateapi::Context c;
  net::Counters n;
  char body[stateapi::kStateBufferBytes];

  // configuredHost is typed by a human into the portal, so it is not trusted here even though it
  // is locally supplied: a quote in it would otherwise produce a document no consumer can parse.
  c.configuredHost = "he said \"hello\" \\ and left";
  stateapi::render(body, sizeof(body), s, c, n);
  CHECK(contains(body, "\\\"hello\\\""), "embedded quotes are escaped");
  CHECK(body[strlen(body) - 1] == '}', "the document still closes");

  // A host far longer than the buffer must truncate rather than overrun.
  char huge[512];
  memset(huge, 'h', sizeof(huge) - 1);
  huge[sizeof(huge) - 1] = '\0';
  c.configuredHost = huge;
  const size_t written = stateapi::render(body, sizeof(body), s, c, n);
  CHECK(written < sizeof(body), "an over-long host does not overrun the buffer");
  CHECK(body[written] == '\0', "the result is NUL-terminated");

  // A control character must not reach the document raw.
  c.configuredHost = "line\nbreak";
  stateapi::render(body, sizeof(body), s, c, n);
  CHECK(!contains(body, "\n\""), "a raw newline does not reach the document");
}

static void test_worst_case_fits_the_buffer() {
  section("the projection fits its buffer with every field at maximum");

  // The endpoint renders into a fixed stack buffer. Nothing had ever measured the worst case, and
  // an estimate put it uncomfortably close: the configured host is escaped, and appendEscaped turns
  // a quote or a backslash into two characters, so a 63-character host of nothing but quotes
  // occupies 126 bytes by itself - twice what a naive count suggests.
  //
  // snprintf truncates safely, so the failure would not be a crash. It would be a JSON document cut
  // off mid-token, which every consumer - the E2E harness included - would report as a transport
  // fault rather than as an overflow. That is the kind of failure that costs an afternoon.
  DisplayState s;
  s.gearPresent = true;
  strcpy(s.gearGlyph, "18");
  s.shift = ShiftPhase::Ramping;
  s.rampPosition = 0.123456f;
  s.barLeft = true;
  s.barRight = true;

  char hostileHost[64];
  memset(hostileHost, '"', sizeof(hostileHost) - 1);    // every character doubles when escaped
  hostileHost[sizeof(hostileHost) - 1] = '\0';

  stateapi::Context c;
  c.configuredHost = hostileHost;
  c.deviceId = "0123456789abcde";
  c.firmwareVersion = "10.20.30-rc999";
  c.linkStateApplies = true;
  c.linkState = LinkState::UnsupportedTitle;            // the longest name in the domain
  c.lastFrameAgePresent = true;
  c.lastFrameAgeMs = 4294967295u;
  c.lastDrawUs = 4294967295u;
  c.worstDrawUs = 4294967295u;
  c.drawCount = 4294967295u;
  c.uptimeMs = 4294967295u;
  c.resetReason = "interruptWatchdog";                  // the longest reason string

  c.backlightOn = true;
  c.blankAfterMinutes = 120;
  c.wakeArmed = false;                                  // "false" is the longer rendering, so both
  c.rigMacKnown = false;                                // booleans take their worst case here

  net::Counters n;
  n.malformed = n.fieldRange = n.outOfOrder = n.versionRejected = n.oversized = 4294967295u;

  char body[stateapi::kStateBufferBytes];
  const size_t written = stateapi::render(body, sizeof(body), s, c, n);

  printf("       worst case: %u of %u bytes\n",
         static_cast<unsigned>(written), static_cast<unsigned>(sizeof(body)));

  CHECK(written < sizeof(body) - 1, "the worst case fits without truncating");
  CHECK(body[written] == '\0', "the result is terminated");
  CHECK(body[written - 1] == '}', "the document is closed, not cut off mid-token");

  // A margin, so the next field added does not silently consume the last of it. If this fails,
  // widen kStateBufferBytes rather than trimming the test.
  CHECK(sizeof(body) - written >= 96, "at least 96 bytes of headroom remain for future fields");
}

int main() {
  printf("API-STATE projection\n");

  test_every_contracted_field_is_present();
  test_unavailable_is_null_not_absent();
  test_link_state_is_null_while_driving();
  test_counters_are_projected();
  test_values_render();
  test_wake_fields_render_both_ways();
  test_hostile_strings_cannot_break_the_json();
  test_worst_case_fits_the_buffer();

  printf("\n%s  %d checks\n", g_failures == 0 ? "PASS" : "FAIL", g_checks);
  if (g_failures != 0) printf("%d failure(s)\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
