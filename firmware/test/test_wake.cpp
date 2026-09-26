// Unit tier for CAP-WAKE-RIG's pure half: the magic packet and the two-touch sequence.
//
// Both are worth asserting here rather than only on the rig, for opposite reasons. The packet is
// exactly checkable and completely unobservable in the field — it goes to a powered-down machine and
// nothing replies — so if it is malformed the only symptom is a rig that does not wake, which has
// four likely causes on another machine before the firmware is even suspected. The sequence is the
// opposite: easy to watch and tedious to provoke, because half its cases need a rig in a particular
// state.
//
// Q16 and Q17 are owed against the device. These checks are what make a failure there mean something
// other than "somewhere in the feature".

#include <cstdio>
#include <cstring>

#include "../src/wake.h"

using namespace cyd;
using namespace cyd::wake;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, what)                                                        \
  do {                                                                           \
    ++g_checks;                                                                   \
    if (!(cond)) { ++g_failures; printf("  FAIL  %s\n", (what)); }                \
  } while (0)

static void section(const char* name) { printf("\n%s\n", name); }

// ---------------------------------------------------------------------------

static void test_packet_layout() {
  section("EVT-WAKE: 102 octets, six 0xFF then the address sixteen times");

  const uint8_t mac[6] = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F};
  uint8_t packet[kMagicPacketBytes + 8];
  memset(packet, 0xAA, sizeof(packet));

  const size_t n = buildMagicPacket(mac, packet, kMagicPacketBytes);
  CHECK(n == 102, "the packet is exactly 102 octets");
  CHECK(kMagicPacketBytes == 102, "the constant agrees with the contract");

  bool syncOk = true;
  for (size_t i = 0; i < 6; ++i) if (packet[i] != 0xFF) syncOk = false;
  CHECK(syncOk, "the first six octets are 0xFF");

  bool repeatsOk = true;
  int repeats = 0;
  for (size_t rep = 0; rep < 16; ++rep) {
    if (memcmp(packet + 6 + rep * 6, mac, 6) != 0) repeatsOk = false; else ++repeats;
  }
  CHECK(repeatsOk, "the address is repeated verbatim");
  CHECK(repeats == 16, "sixteen repetitions, not fifteen and not seventeen");

  // A sync pattern that happens to appear inside the repetitions would make an off-by-one in the
  // layout invisible to the check above, so the boundary is asserted directly.
  CHECK(packet[6] == 0x1A, "the address starts immediately after the sync pattern");
  CHECK(packet[101] == 0x6F, "the last octet is the last octet of the address");
  CHECK(packet[102] == 0xAA, "nothing is written past the packet");
}

static void test_packet_refuses_a_small_buffer() {
  section("a buffer too small yields nothing, never a partial packet");

  const uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
  uint8_t small[64];
  memset(small, 0, sizeof(small));

  // A truncated magic packet is not a broken packet on this wire — it is a well-formed datagram that
  // means nothing, and it would be indistinguishable from a working one from this end, since there is
  // no reply either way. Refusing outright is the only honest failure.
  CHECK(buildMagicPacket(mac, small, sizeof(small)) == 0, "a short buffer is refused");
  bool untouched = true;
  for (size_t i = 0; i < sizeof(small); ++i) if (small[i] != 0) untouched = false;
  CHECK(untouched, "and nothing is written into it");

  CHECK(buildMagicPacket(mac, nullptr, kMagicPacketBytes) == 0, "a null buffer is refused");
}

static void test_first_touch_never_sends() {
  section("the two-touch rule: the first touch offers, the second sends");

  Sequence seq;
  CHECK(!seq.armed(), "a fresh sequence is not armed");

  const TouchAction first = seq.onTouch(1000, LinkState::Unreachable, true);
  CHECK(first == TouchAction::Offered, "the first touch offers");
  CHECK(seq.armed(), "and arms the sequence");

  const TouchAction second = seq.onTouch(2000, LinkState::Unreachable, true);
  CHECK(second == TouchAction::Sent, "the second touch sends");
  CHECK(!seq.armed(), "and disarms - one packet per sequence");

  // A third touch is a new sequence, not a second send. Holding the arming would turn a repeated
  // touch into a stream of packets, which is harmless on the wire and wrong on the glass.
  CHECK(seq.onTouch(2100, LinkState::Unreachable, true) == TouchAction::Offered,
        "a third touch starts over rather than sending again");
}

static void test_offer_lapses() {
  section("the offer window lapses, and a late touch is a first touch");

  Sequence seq;
  seq.onTouch(1000, LinkState::Unreachable, true);
  CHECK(seq.armed(), "armed");

  seq.tick(1000 + kOfferWindowMs - 1);
  CHECK(seq.armed(), "still armed one millisecond inside the window");

  seq.tick(1000 + kOfferWindowMs);
  CHECK(!seq.armed(), "lapsed exactly at the window");

  // The lapse is also enforced inside onTouch, so a loop that missed a tick cannot send on a touch
  // the operator meant as a first one.
  Sequence noTick;
  noTick.onTouch(1000, LinkState::Unreachable, true);
  CHECK(noTick.onTouch(1000 + kOfferWindowMs + 1, LinkState::Unreachable, true) == TouchAction::Offered,
        "a touch after the window offers again rather than sending");
}

static void test_no_learned_address_offers_nothing() {
  section("INV-WAKE-NEEDS-LEARNED-MAC: no address, no offer, no packet");

  Sequence seq;
  const TouchAction a = seq.onTouch(1000, LinkState::Unreachable, false);
  CHECK(a == TouchAction::Lit, "a touch with no learned address only lights the panel");
  CHECK(!seq.armed(), "and does not arm, so a second touch cannot send");

  CHECK(seq.onTouch(2000, LinkState::Unreachable, false) == TouchAction::Lit,
        "a second touch still sends nothing");

  // And the reverse: an address learned between the two touches does not complete a sequence that
  // was never armed.
  CHECK(seq.onTouch(3000, LinkState::Unreachable, true) == TouchAction::Offered,
        "learning the address mid-sequence starts a fresh offer rather than completing one");
}

static void test_the_action_lives_in_one_state() {
  section("the wake is offered in `unreachable` and nowhere else");

  const LinkState others[] = {
    LinkState::Driving, LinkState::DrivingPending, LinkState::Joining, LinkState::Unresolved,
    LinkState::Stale, LinkState::NoSim, LinkState::UnsupportedTitle, LinkState::AdapterFault,
    LinkState::VersionMismatch,
  };

  for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); ++i) {
    Sequence seq;
    const TouchAction a = seq.onTouch(1000, others[i], true);
    CHECK(a == TouchAction::Lit, "a touch outside `unreachable` only lights the panel");
    CHECK(!seq.armed(), "and never arms");
  }

  // `stale` is the one worth naming. From the seat it looks much like `unreachable`, and it was the
  // state I expected to be common - wrongly, because the panel power-cycles with the rig and boots
  // into `unreachable` instead. It means the PC was answering moments ago, so it does not need waking.
  Sequence stale;
  CHECK(stale.onTouch(1000, LinkState::Stale, true) == TouchAction::Lit,
        "`stale` offers nothing: the PC was answering a moment ago");

  // Leaving `unreachable` while armed cannot leave a live offer behind that a later touch completes.
  Sequence left;
  left.onTouch(1000, LinkState::Unreachable, true);
  left.onTouch(1100, LinkState::Driving, true);
  CHECK(!left.armed(), "arming is dropped on leaving the state");
  CHECK(left.onTouch(1200, LinkState::Unreachable, true) == TouchAction::Offered,
        "and returning needs a fresh first touch");
}

static void test_every_touch_lights_the_panel() {
  section("there is no touch that does nothing");

  // Not a tautology worth skipping: the absence of an `Ignored` outcome is the contract. A blanked
  // panel whose touch did nothing in some states would be indistinguishable from a dead one, and
  // lighting on every touch in every state is what makes CAP-BLANK safe to ship.
  const LinkState all[] = {
    LinkState::Driving, LinkState::DrivingPending, LinkState::Joining, LinkState::Unresolved,
    LinkState::Unreachable, LinkState::Stale, LinkState::NoSim, LinkState::UnsupportedTitle,
    LinkState::AdapterFault, LinkState::VersionMismatch,
  };
  for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
    for (int knows = 0; knows < 2; ++knows) {
      Sequence seq;
      const TouchAction a = seq.onTouch(1000, all[i], knows != 0);
      CHECK(a == TouchAction::Lit || a == TouchAction::Offered,
            "every first touch, in every state, at least lights the panel");
    }
  }
}

int main() {
  printf("CAP-WAKE-RIG: the magic packet and the two-touch sequence\n");

  test_packet_layout();
  test_packet_refuses_a_small_buffer();
  test_first_touch_never_sends();
  test_offer_lapses();
  test_no_learned_address_offers_nothing();
  test_the_action_lives_in_one_state();
  test_every_touch_lights_the_panel();

  printf("\n%s  %d checks\n", g_failures == 0 ? "PASS" : "FAIL", g_checks);
  if (g_failures != 0) printf("%d failure(s)\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
