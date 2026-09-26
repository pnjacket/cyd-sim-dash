#include "rigaddr.h"

#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include <lwip/etharp.h>
#include <lwip/netif.h>

namespace cyd {
namespace rigaddr {
namespace {

constexpr const char* kNamespace = "cydrig";
constexpr const char* kKeyMac    = "mac";

uint8_t  g_mac[6]      = {0};
bool     g_known       = false;
uint32_t g_learnedAtMs = 0;
uint32_t g_foreign     = 0;
char     g_macText[18] = {0};

void renderMacText() {
  if (!g_known) { g_macText[0] = 0; return; }
  snprintf(g_macText, sizeof(g_macText), "%02x:%02x:%02x:%02x:%02x:%02x",
           g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);
}

void store() {
  Preferences p;
  if (!p.begin(kNamespace, /*readOnly=*/false)) return;
  p.putBytes(kKeyMac, g_mac, sizeof(g_mac));
  p.end();
}

}  // namespace

void begin() {
  Preferences p;
  if (!p.begin(kNamespace, /*readOnly=*/true)) return;
  const size_t got = p.getBytes(kKeyMac, g_mac, sizeof(g_mac));
  p.end();

  // An all-zero address is not a valid destination, and is what a partially written record looks
  // like. Treated as absent, so the panel offers nothing rather than sending to nowhere.
  if (got == sizeof(g_mac)) {
    for (size_t i = 0; i < sizeof(g_mac); ++i) {
      if (g_mac[i] != 0) { g_known = true; break; }
    }
  }
  if (!g_known) memset(g_mac, 0, sizeof(g_mac));
  renderMacText();
}

bool known() { return g_known; }
const uint8_t* mac() { return g_mac; }
uint32_t learnedAtMs() { return g_learnedAtMs; }
const char* macText() { return g_macText; }
uint32_t foreignSenderCount() { return g_foreign; }

void forget() {
  memset(g_mac, 0, sizeof(g_mac));
  g_known = false;
  g_learnedAtMs = 0;
  g_macText[0] = 0;

  Preferences p;
  if (!p.begin(kNamespace, /*readOnly=*/false)) return;
  // Overwritten before removal, for the same reason config::erase() does it: deleting a key can leave
  // the old bytes in flash, and this one names a machine.
  const uint8_t blank[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  p.putBytes(kKeyMac, blank, sizeof(blank));
  p.remove(kKeyMac);
  p.end();
}

bool observe(const IPAddress& sender, const IPAddress& expected) {
  // Only the configured host teaches this device anything.
  //
  // The wire contract does not bind a frame to a source, so a frame from elsewhere on the LAN is
  // perfectly valid and is rendered as usual. What it must not do is redefine what a wake is aimed
  // at. Counted rather than dropped quietly: something other than the configured host feeding this
  // panel is worth being able to see.
  if (sender != expected) {
    ++g_foreign;
    return g_known;
  }

  // The ARP cache is the stack's own record of who answered at that address. Reading it rather than
  // probing means learning costs nothing and cannot fail in a way that affects the receive path:
  // either the entry is there because the PC has been talking to us, or it is not and we keep
  // whatever we had.
  ip4_addr_t target;
  IP4_ADDR(&target, sender[0], sender[1], sender[2], sender[3]);

  struct eth_addr* eth = nullptr;
  const ip4_addr_t* ip = nullptr;
  if (etharp_find_addr(netif_default, &target, &eth, &ip) < 0 || eth == nullptr) {
    return g_known;
  }

  const bool changed = !g_known || memcmp(g_mac, eth->addr, sizeof(g_mac)) != 0;
  memcpy(g_mac, eth->addr, sizeof(g_mac));
  g_known = true;
  g_learnedAtMs = millis();
  renderMacText();

  if (changed) store();
  return true;
}

}  // namespace rigaddr
}  // namespace cyd
