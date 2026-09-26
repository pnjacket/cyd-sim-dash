#include "rigaddr.h"

#include <Arduino.h>
#include <Preferences.h>
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
}

bool known() { return g_known; }
const uint8_t* mac() { return g_mac; }
uint32_t learnedAtMs() { return g_learnedAtMs; }

bool observe(const IPAddress& addr) {
  // The ARP cache is the stack's own record of who answered at that address. Reading it rather than
  // probing means learning costs nothing and cannot fail in a way that affects the receive path:
  // either the entry is there because the PC has been talking to us, or it is not and we keep
  // whatever we had.
  ip4_addr_t target;
  IP4_ADDR(&target, addr[0], addr[1], addr[2], addr[3]);

  struct eth_addr* eth = nullptr;
  const ip4_addr_t* ip = nullptr;
  if (etharp_find_addr(netif_default, &target, &eth, &ip) < 0 || eth == nullptr) {
    return g_known;
  }

  const bool changed = !g_known || memcmp(g_mac, eth->addr, sizeof(g_mac)) != 0;
  memcpy(g_mac, eth->addr, sizeof(g_mac));
  g_known = true;
  g_learnedAtMs = millis();

  if (changed) store();
  return true;
}

}  // namespace rigaddr
}  // namespace cyd
