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

  char body[640];
  stateapi::render(body, sizeof(body), s, c, n);

  // The contract lists these by name. A field that vanishes when its value is empty is the classic
  // way a consumer starts guessing, so presence is asserted unconditionally.
  const char* required[] = {
    "\"gearGlyph\"", "\"shiftPhase\"", "\"rampPosition\"", "\"barLeft\"", "\"barRight\"",
    "\"linkState\"", "\"configuredHost\"", "\"lastFrameAgeMs\"",
    "\"malformedCount\"", "\"fieldRangeCount\"", "\"outOfOrderCount\"", "\"versionRejectedCount\"",
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
  char body[640];
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
  char body[640];

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

  char body[640];
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
  char body[640];
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

static void test_hostile_strings_cannot_break_the_json() {
  section("text from outside cannot break the document");

  DisplayState s;
  stateapi::Context c;
  net::Counters n;
  char body[640];

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

int main() {
  printf("API-STATE projection\n");

  test_every_contracted_field_is_present();
  test_unavailable_is_null_not_absent();
  test_link_state_is_null_while_driving();
  test_counters_are_projected();
  test_values_render();
  test_hostile_strings_cannot_break_the_json();

  printf("\n%s  %d checks\n", g_failures == 0 ? "PASS" : "FAIL", g_checks);
  if (g_failures != 0) printf("%d failure(s)\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
