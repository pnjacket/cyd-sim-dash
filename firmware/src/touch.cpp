#include "touch.h"

#include <Arduino.h>
#include <SPI.h>

namespace cyd {
namespace touch {
namespace {

// The touch controller's own peripheral. The display holds HSPI; this is the other one.
SPIClass g_spi(VSPI);
bool     g_begun = false;

int      g_pressure   = 0;
bool     g_held       = false;     // a press has been reported and not yet released
uint32_t g_downSince  = 0;         // when pressure first crossed the floor
uint32_t g_upSince    = 0;         // when it last fell below it
bool     g_above      = false;

// XPT2046 control bytes. Bit 7 starts a conversion; bits 6-4 select the channel. The device answers
// the PREVIOUS request while the next one is being clocked in, so each read carries the following
// command — which is why these look off by one.
constexpr uint8_t kCmdZ1 = 0xB1;
constexpr uint8_t kCmdZ2 = 0xC1;
constexpr uint8_t kCmdIdle = 0x00;   // no new conversion; clocks out the last result

// SOURCE: adapted from XPT2046_Touchscreen by Paul Stoffregen (MIT). Attested in PROVENANCE.md.
// The control bytes come from the datasheet; what is adapted is the Z1/Z2 combination, the
// pipelining idiom below, and the pressure floor in the header.
//
// Derived pressure. Touch resistance is measured across two channels, and the useful quantity is
// their difference rather than either alone: Z1 rises with contact and Z2 falls, so combining them
// cancels most of the variation with position across the panel.
int readPressure() {
  g_spi.beginTransaction(SPISettings(kSpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(kCs, LOW);

  g_spi.transfer(kCmdZ1);                              // request Z1
  const int z1 = g_spi.transfer16(kCmdZ2) >> 3;        // read Z1, request Z2
  const int z2 = g_spi.transfer16(kCmdIdle) >> 3;      // read Z2

  digitalWrite(kCs, HIGH);
  g_spi.endTransaction();

  return z1 + 4095 - z2;
}

}  // namespace

void begin() {
  if (g_begun) return;
  pinMode(kCs, OUTPUT);
  digitalWrite(kCs, HIGH);
  g_spi.begin(kSck, kMiso, kMosi, kCs);
  g_begun = true;
}

int lastPressure() { return g_pressure; }

bool pressed(uint32_t nowMs) {
  if (!g_begun) return false;

  g_pressure = readPressure();
  const bool above = g_pressure >= kPressureFloor;

  if (above != g_above) {
    g_above = above;
    if (above) g_downSince = nowMs; else g_upSince = nowMs;
  }

  if (g_held) {
    // Released only after the glass has been clear for long enough. A resistive panel chatters on
    // release, and a short window here would report a second press from the tail of the first.
    if (!above && (nowMs - g_upSince) >= kReleaseMs) g_held = false;
    return false;
  }

  if (above && (nowMs - g_downSince) >= kHoldMs) {
    g_held = true;
    return true;
  }
  return false;
}

}  // namespace touch
}  // namespace cyd
