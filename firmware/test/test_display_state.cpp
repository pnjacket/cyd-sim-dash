// Unit tier for the display-state engine.
//
// Deliberately framework-free: CI needs a C++ compiler and nothing else. Each test names the
// contract or acceptance check it proves, so a failure points at a document rather than at a line
// number.

#include "../src/display_state.h"

#include <stdio.h>
#include <string.h>

// Required by the range-for over a braced list below. Pulled in transitively by libstdc++ on some
// toolchains and not on others, which is exactly the kind of latent break that survives until the
// first machine that compiles it - this file had never been compiled when the omission was found.
#include <initializer_list>

using namespace cyd;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, what)                                                        \
  do {                                                                           \
    ++g_checks;                                                                  \
    if (!(cond)) {                                                               \
      ++g_failures;                                                              \
      printf("  FAIL  %s\n        at %s:%d\n", (what), __FILE__, __LINE__);      \
    }                                                                            \
  } while (0)

static void section(const char* name) { printf("\n%s\n", name); }

static Frame liveFrame() {
  Frame f;
  f.protocolMajor = kProtocolMajor;
  f.protocolMinor = 0;
  f.status = Status::Live;
  strcpy(f.titleId, "iRacing");
  f.stamp = 41250;
  f.gearPresent = true; strcpy(f.gear, "4");
  f.rpmPresent = true;  f.rpm = 7200.0f;
  f.rampPresent = true; f.rampStartRpm = 7800.0f;
  f.flashPresent = true; f.flashRpm = 8400.0f;
  f.spotterLeft = Spotter::One;
  f.spotterRight = Spotter::None;
  return f;
}

static Frame idleFrame(Status s) {
  Frame f;
  f.protocolMajor = kProtocolMajor;
  f.status = s;
  f.stamp = 52310;
  if (s == Status::UnsupportedTitle) strcpy(f.titleId, "AssettoCorsa");
  return f;  // every telemetry element left absent
}

static LinkInputs liveInputs() {
  LinkInputs in;
  in.wifiAssociated = true;
  in.hostResolved = true;
  in.everAccepted = true;
  in.msSinceAccepted = 50;
  in.msSinceRegister = 30000;
  return in;
}

// ---------------------------------------------------------------------------

static void test_gear_domain() {
  section("INV-GEAR-DOMAIN  (A2, D1)");
  CHECK(gearInDomain("R"), "R is in domain");
  CHECK(gearInDomain("N"), "N is in domain");
  CHECK(gearInDomain("1"), "1 is in domain");
  CHECK(gearInDomain("9"), "9 is in domain");
  CHECK(gearInDomain("10"), "10 is in domain");
  CHECK(gearInDomain("18"), "18 is in domain — the truck case, kept so a v2 adapter needs no firmware change");
  CHECK(!gearInDomain("0"), "0 is not a gear");
  CHECK(!gearInDomain("19"), "19 is above the domain");
  CHECK(!gearInDomain("100"), "three characters is out of domain");
  CHECK(!gearInDomain(""), "empty is out of domain");
  CHECK(!gearInDomain("X"), "arbitrary letters are out of domain");
}

static void test_validation() {
  section("Frame validation  (D1, I6)");
  Frame f = liveFrame();
  CHECK(validate(f) == Reject::Accepted, "a typical live frame validates");

  f = liveFrame(); f.protocolMajor = kProtocolMajor + 1;
  CHECK(validate(f) == Reject::VersionMajor, "INV-VERSION-WHOLE: a differing major is refused");

  f = liveFrame(); f.protocolMinor = 7;
  CHECK(validate(f) == Reject::Accepted, "a differing minor is tolerated");

  f = liveFrame(); f.rpm = -1.0f;
  CHECK(validate(f) == Reject::FieldRange, "INV-RPM-NONNEG: negative rpm is rejected");

  f = liveFrame(); strcpy(f.gear, "19");
  CHECK(validate(f) == Reject::FieldRange, "INV-GEAR-DOMAIN: gear 19 is rejected");

  f = liveFrame(); f.rampStartRpm = 8400.0f; f.flashRpm = 7800.0f;
  CHECK(validate(f) == Reject::FieldRange, "INV-RAMP-ORDER: inverted thresholds are rejected");

  f = liveFrame(); f.rampStartRpm = 8000.0f; f.flashRpm = 8000.0f;
  CHECK(validate(f) == Reject::FieldRange, "INV-RAMP-ORDER: equal thresholds are rejected, not treated as degenerate");

  f = liveFrame(); f.flashPresent = false;
  CHECK(validate(f) == Reject::FieldRange, "a half-present threshold pair is rejected");
}

static void test_status_consistency() {
  section("INV-STATUS-CONSISTENT  (D13)");
  for (Status s : {Status::NoSim, Status::UnsupportedTitle, Status::AdapterFault}) {
    Frame f = idleFrame(s);
    CHECK(validate(f) == Reject::Accepted, "a non-live frame with everything unavailable validates");
    f.gearPresent = true; strcpy(f.gear, "4");
    CHECK(validate(f) == Reject::StatusInconsistent, "a non-live frame carrying a gear is rejected whole");
  }
  Frame f = idleFrame(Status::UnsupportedTitle);
  CHECK(strcmp(f.titleId, "AssettoCorsa") == 0 && validate(f) == Reject::Accepted,
        "titleId is exempt: an unsupportedTitle frame must be able to name the title");
}

static void test_stamp_ordering() {
  section("INV-STAMP-ORDER  (D2, D3)");
  StampTracker t;
  CHECK(t.accept(1000), "the first frame is always accepted");
  CHECK(t.accept(1016), "a later stamp is accepted");
  CHECK(!t.accept(1016), "a duplicate stamp is rejected");
  CHECK(!t.accept(1008), "a transposed frame is rejected");
  CHECK(t.accept(1032), "ordering resumes after a rejection");

  // The producer-restart case. Without this rule the device dies silently and permanently.
  StampTracker r;
  r.accept(500000);
  CHECK(!r.accept(499000), "a small backwards step is reordering, not a restart");
  CHECK(r.accept(120), "a backwards jump beyond the staleness threshold is a producer restart");
  CHECK(r.accept(136), "and ordering continues from the new run");
}

static void test_ramp_position() {
  section("Ramp position  (U4)");
  CHECK(rampPosition(7800, 7800, 8400) == 0.0f, "at the ramp start the position is 0");
  CHECK(rampPosition(8400, 7800, 8400) == 1.0f, "at the flash point the position is 1");
  const float mid = rampPosition(8100, 7800, 8400);
  CHECK(mid > 0.49f && mid < 0.51f, "halfway through the window is 0.5");
  CHECK(rampPosition(7000, 7800, 8400) == 0.0f, "below the window it clamps to 0");
  CHECK(rampPosition(9000, 7800, 8400) == 1.0f, "above the window it clamps to 1");
  CHECK(rampPosition(8000, 8400, 8400) == 0.0f, "a degenerate window does not divide by zero");
}

static void test_shift_phase() {
  section("Shift phase  (A3, U4, U5)");
  Frame f = liveFrame();
  LinkInputs in = liveInputs();

  f.rpm = 7000.0f;
  CHECK(derive(f, in).shift == ShiftPhase::Neutral, "below the ramp start the background is neutral");

  f.rpm = 8100.0f;
  DisplayState s = derive(f, in);
  CHECK(s.shift == ShiftPhase::Ramping, "inside the window the background ramps");
  CHECK(s.rampPosition > 0.49f && s.rampPosition < 0.51f, "and the position reflects how far through");

  f.rpm = 8400.0f;
  CHECK(derive(f, in).shift == ShiftPhase::Flashing, "at the flash point it flashes");

  f.rpm = 9500.0f;
  CHECK(derive(f, in).shift == ShiftPhase::Flashing,
        "and keeps flashing above it — no over-rev state, no time-out");

  f = liveFrame(); f.rampPresent = false; f.flashPresent = false;
  CHECK(derive(f, in).shift == ShiftPhase::Unavailable,
        "with no thresholds the shift element is unavailable, never invented");
}

static void test_bars() {
  section("Edge bars  (U7, X3)");
  Frame f = liveFrame();
  LinkInputs in = liveInputs();

  f.spotterLeft = Spotter::One; f.spotterRight = Spotter::None;
  DisplayState s = derive(f, in);
  CHECK(s.barLeft && !s.barRight, "one car on the left lights only the left bar");

  f.spotterLeft = Spotter::One; f.spotterRight = Spotter::One;
  s = derive(f, in);
  CHECK(s.barLeft && s.barRight, "both sides light independently and simultaneously");

  f.spotterLeft = Spotter::Two; f.spotterRight = Spotter::None;
  CHECK(derive(f, in).barLeft, "two cars on a side still lights the bar");

  f.spotterLeft = Spotter::None; f.spotterRight = Spotter::None;
  s = derive(f, in);
  CHECK(!s.barLeft && !s.barRight, "clear leaves both dark");

  f.spotterLeft = Spotter::Unavailable; f.spotterRight = Spotter::Unavailable;
  s = derive(f, in);
  CHECK(!s.barLeft && !s.barRight, "unavailable also renders dark — but is a different fact in the data");
}

static void test_link_ladder() {
  section("linkState ladder  (U8, first match wins)");
  LinkInputs in;

  in = liveInputs(); in.wifiAssociated = false;
  CHECK(deriveLink(in, Status::Live) == LinkState::Joining, "1. no WiFi outranks everything");

  in = liveInputs(); in.hostResolved = false;
  CHECK(deriveLink(in, Status::Live) == LinkState::Unresolved, "2. unresolved host outranks frame contents");

  in = liveInputs(); in.versionRejected = true;
  CHECK(deriveLink(in, Status::Live) == LinkState::VersionMismatch, "3. a version refusal outranks freshness");

  in = liveInputs(); in.everAccepted = false; in.msSinceRegister = 1000;
  CHECK(deriveLink(in, Status::Live) == LinkState::DrivingPending, "4. inside the grace period, we are still starting up");

  in = liveInputs(); in.everAccepted = false; in.msSinceRegister = kFirstFrameGraceMs + 1;
  CHECK(deriveLink(in, Status::Live) == LinkState::Unreachable, "5. past the grace period, the PC side is not there");

  in = liveInputs(); in.msSinceAccepted = kStalenessMs + 1;
  CHECK(deriveLink(in, Status::Live) == LinkState::Stale, "6. frames stopped after having flowed");

  in = liveInputs();
  CHECK(deriveLink(in, Status::NoSim) == LinkState::NoSim, "7. a fresh noSim frame");
  CHECK(deriveLink(in, Status::UnsupportedTitle) == LinkState::UnsupportedTitle, "8. a fresh unsupportedTitle frame");
  CHECK(deriveLink(in, Status::AdapterFault) == LinkState::AdapterFault, "9. a fresh adapterFault frame");
  CHECK(deriveLink(in, Status::Live) == LinkState::Driving, "a fresh live frame yields no link state at all");

  // 6b. Stale decays, and the boundary is asserted from both sides.
  //
  // This is the rung that was missing, and the defect it caused is worth keeping in view: every other
  // rung was individually correct, so nothing at the unit tier was wrong. What was wrong was that one
  // condition had no exit, and the consequence landed two contracts away - CAP-WAKE-RIG arms only in
  // Unreachable, so a panel that had been driving and then lost its PC could never offer a wake.
  in = liveInputs(); in.msSinceAccepted = kStaleDecayMs - 1;
  CHECK(deriveLink(in, Status::Live) == LinkState::Stale, "6. one millisecond inside the decay period");

  in = liveInputs(); in.msSinceAccepted = kStaleDecayMs;
  CHECK(deriveLink(in, Status::Live) == LinkState::Unreachable, "6b. exactly at the decay period");

  in = liveInputs(); in.msSinceAccepted = 4294967295u;
  CHECK(deriveLink(in, Status::Live) == LinkState::Unreachable,
        "6b. and it stays there - stale is a transient, never a resting place");

  // The decayed answer must match what a panel that had never accepted a frame would say, because that
  // is the whole point: the screen should not depend on whether this panel happened to be powered up
  // before the PC went down.
  LinkInputs never = liveInputs();
  never.everAccepted = false;
  never.msSinceRegister = kFirstFrameGraceMs + 1;
  LinkInputs decayed = liveInputs();
  decayed.msSinceAccepted = kStaleDecayMs + 1;
  CHECK(deriveLink(never, Status::Live) == deriveLink(decayed, Status::Live),
        "a decayed panel and a freshly booted one give the same answer");

  // Decay must not outrank the link-layer rungs above it. A decayed panel that has also lost WiFi is
  // joining, not unreachable - otherwise the ladder would start telling you about the PC when the
  // problem is the network in front of it.
  in = liveInputs(); in.msSinceAccepted = kStaleDecayMs + 1; in.wifiAssociated = false;
  CHECK(deriveLink(in, Status::Live) == LinkState::Joining, "row 1 still outranks the decay");
  in = liveInputs(); in.msSinceAccepted = kStaleDecayMs + 1; in.hostResolved = false;
  CHECK(deriveLink(in, Status::Live) == LinkState::Unresolved, "and so does row 2");
  in = liveInputs(); in.msSinceAccepted = kStaleDecayMs + 1; in.versionRejected = true;
  CHECK(deriveLink(in, Status::Live) == LinkState::VersionMismatch, "and row 3");

  // The precedence question the docs were asked to answer.
  in = liveInputs(); in.msSinceAccepted = 4000;
  CHECK(deriveLink(in, Status::AdapterFault) == LinkState::Stale,
        "a four-second-old adapterFault reads as Stale — an old fault says nothing about now");
}

static void test_freshness_gates_rendering() {
  section("INV-FRESH-RENDER  (D4, A5)");
  Frame f = liveFrame();
  LinkInputs in = liveInputs();

  in.msSinceAccepted = 1500;
  DisplayState s = derive(f, in);
  CHECK(s.link == LinkState::Driving && s.gearPresent, "within the threshold the panel keeps driving");

  in.msSinceAccepted = 2500;
  s = derive(f, in);
  CHECK(s.link == LinkState::Stale, "past the threshold it falls to link state");
  CHECK(!s.gearPresent && s.shift == ShiftPhase::Unavailable && !s.barLeft && !s.barRight,
        "and stops asserting stale values entirely");
}

static void test_no_per_title_branch() {
  section("COMPONENT-STATE: no per-title branch");
  Frame a = liveFrame();  strcpy(a.titleId, "iRacing");
  Frame b = liveFrame();  strcpy(b.titleId, "SomeOtherSim");
  LinkInputs in = liveInputs();
  DisplayState sa = derive(a, in), sb = derive(b, in);
  CHECK(sa.shift == sb.shift && sa.barLeft == sb.barLeft && sa.gearPresent == sb.gearPresent &&
            sa.rampPosition == sb.rampPosition,
        "identical frames differing only in title produce identical display state");
}

static void test_flash_cadence() {
  section("the flash runs at 3 Hz, 50% duty, derived from the clock");

  // U5's instrumented measurement was waived by the operator on 2026-09-23, who judged the rate
  // acceptable by eye. So this is now the ONLY evidence for the rate: it proves the arithmetic,
  // and nothing proves the panel matches it.
  CHECK(kFlashPeriodMs == 333, "the period is 333 ms, i.e. 3 Hz");
  const uint32_t period = kFlashPeriodMs;

  CHECK(flashOn(0), "the period opens lit");
  CHECK(flashOn(165), "still lit just before the half-period");
  CHECK(!flashOn(166), "dark from the half-period");
  CHECK(!flashOn(332), "still dark at the end of the period");
  CHECK(flashOn(333), "lit again at the start of the next period");

  // Duty cycle across a long run: half lit, within a period's worth of rounding.
  int lit = 0;
  const int span = kFlashPeriodMs * 30;
  for (int t = 0; t < span; ++t) {
    if (flashOn(static_cast<uint32_t>(t))) ++lit;
  }
  const double duty = static_cast<double>(lit) / span;
  CHECK(duty > 0.49 && duty < 0.51, "the duty cycle is 50% across thirty periods");

  // Derived from the clock, not toggled: sampling irregularly - as a loop with a variable frame
  // time really does - must not shift the cadence. A toggle-based implementation would drift here.
  CHECK(flashOn(10 * period + 10), "phase is preserved ten periods later");
  CHECK(!flashOn(10 * period + 200), "and so is the dark half");
  CHECK(flashOn(1000 * period), "and a thousand periods later");
}

static void test_backlight_rule() {
  section("CAP-BLANK: when the backlight is on");

  const uint32_t min = 60000;

  // Driving always lights it, however long the timer says.
  CHECK(backlightShouldBeOn(LinkState::Driving, false, 999 * min, 1), "driving is always lit");

  // The ordinary case: a non-driving state blanks once the period passes.
  CHECK(backlightShouldBeOn(LinkState::Unreachable, false, 0, 1), "lit immediately after driving stops");
  CHECK(backlightShouldBeOn(LinkState::Unreachable, false, min - 1, 1), "still lit just under the period");
  CHECK(!backlightShouldBeOn(LinkState::Unreachable, false, min, 1), "dark at the period");
  CHECK(!backlightShouldBeOn(LinkState::Unreachable, false, 10 * min, 1), "and stays dark");

  // Every non-driving condition blanks - the operator chose the union, not a subset.
  const LinkState blanking[] = {
      LinkState::Unreachable, LinkState::NoSim, LinkState::Stale, LinkState::AdapterFault,
      LinkState::Unresolved, LinkState::Joining, LinkState::DrivingPending,
      LinkState::UnsupportedTitle,
  };
  for (LinkState l : blanking) {
    CHECK(!backlightShouldBeOn(l, false, 5 * min, 1), "this condition blanks");
  }

  // The two exemptions.
  CHECK(backlightShouldBeOn(LinkState::VersionMismatch, false, 999 * min, 1),
        "versionMismatch stays lit - it is the one state that must be read");
  CHECK(backlightShouldBeOn(LinkState::Unreachable, true, 999 * min, 1),
        "an update in progress stays lit - a dark panel mid-transfer invites pulling the power");

  // 0 disables. This is the escape hatch, so it is asserted against a deliberately absurd idle time.
  CHECK(backlightShouldBeOn(LinkState::Unreachable, false, 4294967295u, 0),
        "0 never blanks, even at the maximum representable idle");

  // The longest configurable period still behaves, and does not wrap. 120 x 60000 is 7,200,000; a
  // 32-bit multiply would survive that but the next person to raise the bound might not notice.
  CHECK(backlightShouldBeOn(LinkState::Unreachable, false, 119 * min, 120), "just under 120 minutes is lit");
  CHECK(!backlightShouldBeOn(LinkState::Unreachable, false, 120 * min, 120), "dark at 120 minutes");
}

static void test_link_lines_cover_every_condition() {
  section("SCREEN-LINK: every condition has a line, and it reads as a sentence");

  const LinkState all[] = {
    LinkState::DrivingPending, LinkState::Joining, LinkState::Unresolved, LinkState::Unreachable,
    LinkState::Stale, LinkState::NoSim, LinkState::UnsupportedTitle, LinkState::AdapterFault,
    LinkState::VersionMismatch,
  };

  LinkText text;
  for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
    char line[64];
    const size_t n = linkLineInto(line, sizeof(line), all[i], text);
    CHECK(n > 0, "the condition has a line");
    CHECK(line[n] == '\0', "and it is terminated");

    // Every line is a complete sentence rather than a label, because an adopter arrives knowing none
    // of these glyphs and the icon alone teaches them nothing. A bare word would be a label.
    CHECK(n >= 9, "the line says something, rather than labelling the state");
  }

  // Driving is the one state with no line: the driving screen shows instead. An empty string here is
  // correct and a placeholder would be worse - it would put text on a screen that has none.
  char driving[64];
  CHECK(linkLineInto(driving, sizeof(driving), LinkState::Driving, text) == 0,
        "driving has no line, because it has no link screen");
}

static void test_the_two_interpolating_lines() {
  section("the two lines that carry a value carry it");

  char line[64];

  // Naming the title is the entire point of the unsupported-title condition. Without it the line says
  // a title is unsupported and leaves you to guess which one the plugin thinks is running - and the
  // most likely reason for seeing this screen at all is that it has the wrong one.
  LinkText title;
  title.titleId = "assettocorsa";
  linkLineInto(line, sizeof(line), LinkState::UnsupportedTitle, title);
  CHECK(strstr(line, "assettocorsa") != nullptr, "the unsupported title is named");
  CHECK(strstr(line, "not supported") != nullptr, "and the line still says what is wrong");

  // Both versions, because the condition does not say which half to update and that is the only
  // action available. One number would be worse than none: it reads as the wrong half as often as the
  // right one.
  LinkText version;
  version.deviceMajor = 1;
  version.deviceMinor = 0;
  version.peerVersionKnown = true;
  version.peerMajor = 2;
  version.peerMinor = 1;
  linkLineInto(line, sizeof(line), LinkState::VersionMismatch, version);
  CHECK(strstr(line, "1.0") != nullptr, "the device's version appears");
  CHECK(strstr(line, "2.1") != nullptr, "and the plugin's");

  // The order matters, and is not arbitrary: the contract says device then plugin. Reversed, the line
  // would be read confidently and wrongly, and the operator would update the half that was already
  // current.
  const char* device = strstr(line, "1.0");
  const char* peer = strstr(line, "2.1");
  CHECK(device != nullptr && peer != nullptr && device < peer,
        "the device's version comes first, as the contract specifies");
}

static void test_missing_values_fall_back_rather_than_print_a_hole() {
  section("a missing value falls back to the generic wording");

  char line[64];

  // A line reading "not supported" with nothing in front of it, or "mismatch - 1.0 vs 0.0", is worse
  // than the generic wording: the first looks broken and the second is a specific claim that happens
  // to be false. 0.0 is not a version anything ever reported.
  LinkText none;
  linkLineInto(line, sizeof(line), LinkState::UnsupportedTitle, none);
  CHECK(strcmp(line, "Title not supported") == 0, "no title falls back to the generic line");

  LinkText empty;
  empty.titleId = "";
  linkLineInto(line, sizeof(line), LinkState::UnsupportedTitle, empty);
  CHECK(strcmp(line, "Title not supported") == 0, "an empty title does too, not a leading space");

  linkLineInto(line, sizeof(line), LinkState::VersionMismatch, none);
  CHECK(strcmp(line, "Version mismatch") == 0, "an unknown peer version falls back");
  CHECK(strstr(line, "0.0") == nullptr, "and never prints a version nothing reported");
}

static void test_the_line_cannot_overrun_or_be_broken_by_the_wire() {
  section("the title comes off the wire, so it is not trusted for length");

  char line[64];

  char huge[64];
  memset(huge, 'z', sizeof(huge) - 1);
  huge[sizeof(huge) - 1] = '\0';

  LinkText text;
  text.titleId = huge;
  const size_t n = linkLineInto(line, sizeof(line), LinkState::UnsupportedTitle, text);
  CHECK(n < sizeof(line), "an over-long title does not overrun the buffer");
  CHECK(line[n] == '\0', "the result is terminated");

  // Bounded well inside the buffer rather than merely not overrunning it, so the words after the
  // title survive. A line truncated to nothing but a title name says less than the generic wording.
  CHECK(strstr(line, "not supported") != nullptr, "and the words after it survive");

  // A buffer too small to hold anything must still leave a valid empty string rather than whatever
  // was on the stack.
  char tiny[1];
  CHECK(linkLineInto(tiny, sizeof(tiny), LinkState::NoSim, text) == 0, "a one-byte buffer writes nothing");
  CHECK(tiny[0] == '\0', "and is still a valid string");
  CHECK(linkLineInto(nullptr, 64, LinkState::NoSim, text) == 0, "a null buffer is refused");
}

static void test_the_uninterpolated_line_matches_the_interpolated_one() {
  section("the serial log and the glass cannot show different wording");

  // linkLine() is what the serial log prints and linkLineInto() is what the panel draws. Two copies
  // of nine strings is two copies that can drift, and the one on the glass is the one nobody diffs.
  // So for every condition that interpolates nothing, they must be the same text.
  const LinkState plain[] = {
    LinkState::DrivingPending, LinkState::Joining, LinkState::Unresolved, LinkState::Unreachable,
    LinkState::Stale, LinkState::NoSim, LinkState::AdapterFault,
  };
  LinkText text;
  for (size_t i = 0; i < sizeof(plain) / sizeof(plain[0]); ++i) {
    char line[64];
    linkLineInto(line, sizeof(line), plain[i], text);
    CHECK(strcmp(line, linkLine(plain[i])) == 0, "the two renderings agree");
  }
}

int main() {
  printf("display-state engine — unit tier\n");
  test_gear_domain();
  test_validation();
  test_status_consistency();
  test_stamp_ordering();
  test_ramp_position();
  test_shift_phase();
  test_bars();
  test_link_ladder();
  test_freshness_gates_rendering();
  test_no_per_title_branch();
  test_flash_cadence();
  test_backlight_rule();

  test_link_lines_cover_every_condition();
  test_the_two_interpolating_lines();
  test_missing_values_fall_back_rather_than_print_a_hole();
  test_the_line_cannot_overrun_or_be_broken_by_the_wire();
  test_the_uninterpolated_line_matches_the_interpolated_one();

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
