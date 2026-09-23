// The one translation unit that includes TFT_eSPI.h — and it does so only after tft_config.h,
// which is what keeps the library's own User_Setup.h out of the picture entirely.
#include "tft_config.h"

#include <TFT_eSPI.h>

#include "panel.h"

#include <string.h>

namespace cyd {
namespace panel {
namespace {

TFT_eSPI tft;

// What is currently on the glass. Redrawing only the changed elements keeps a steady scene free:
// a full repaint at frame rate would tear visibly over SPI and starve the receive path.
struct Shown {
  bool     valid       = false;
  uint16_t background  = 0;
  bool     gearPresent = false;
  char     gearGlyph[3] = {0};
  bool     barLeft     = false;
  bool     barRight    = false;
};
Shown g_shown;

// Draw timing, in microseconds. Reported through API-STATE.
uint32_t g_lastDrawUs = 0;
uint32_t g_worstDrawUs = 0;

// How many full redraws have happened. Reported through API-STATE so the effect of drawing less
// often is a measured number rather than an impression - the previous two attempts at this problem
// were both judged by eye and both fixed the wrong thing.
uint32_t g_drawCount = 0;

// RGB565 channel-wise blend. Interpolating in 565 space rather than converting to 888 and back
// is slightly cruder but costs nothing on an ESP32 and is imperceptible across a ramp this wide.
uint16_t blend(uint16_t a, uint16_t b, float t) {
  if (t <= 0.0f) return a;
  if (t >= 1.0f) return b;
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + static_cast<int>((br - ar) * t + 0.5f);
  const int g = ag + static_cast<int>((bg - ag) * t + 0.5f);
  const int bl = ab + static_cast<int>((bb - ab) * t + 0.5f);
  return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

// The text multiplier for the gear glyph. Computed once, then used for every gear.
//
// Two things were making the glyph smaller than it needed to be.
//
// First, fontHeight() on a GFX font returns yAdvance - the whole line box, sized for ascenders and
// descenders that a gear glyph never uses. A digit's ink is roughly 62% of that. Sizing against the
// line box reserved space for parts of the font that are never drawn, and cost a whole multiplier
// step. The fraction is an approximation of cap height over yAdvance for the Free* faces; it is
// used only to choose a size, and the result is confirmed on the glass.
//
// Second, sizing per-glyph would make a one-character gear larger than a two-character one, so the
// display would change size as well as content while shifting. Sizing once against the WIDEST value
// in the domain gives every gear the same size, which is what an instrument should do - and it
// makes U3 hold for the whole domain rather than for whichever value happened to be on screen.
uint8_t gearTextSize() {
  static uint8_t cached = 0;
  if (cached != 0) return cached;

  const char* widest = "18";                   // the widest value the gear domain contains
  const int availableH = kHeight - 2 * kBandHeight - 8;
  const int availableW = kGearWidth - 16;

  cached = 1;
  for (uint8_t candidate = 6; candidate >= 1; --candidate) {
    tft.setTextSize(candidate);
    const int inkH = (tft.fontHeight() * 62) / 100;
    if (inkH <= availableH && tft.textWidth(widest) <= availableW) {
      cached = candidate;
      break;
    }
  }
  return cached;
}

}  // namespace

uint16_t rampColour(float position) {
  // Four discrete stops rather than a continuous blend.
  //
  // A blend changes colour on every RPM step, and every colour change is a full band repaint - one
  // visible sweep across the glass. Four stops means at most four repaints across the entire ramp,
  // however fast the engine is revving, which is the difference between a panel that is constantly
  // mid-sweep and one that changes a handful of times.
  //
  // It also reads more like a shift light than a wash does: discrete stages are what a driver is
  // used to reading peripherally, and the exact shade between two stages carried no information a
  // driver could act on anyway.
  //
  // Chosen by the operator on 2026-09-22; it amends U4, which previously required a continuous
  // blend with no visible banding. The banding is now the point.
  if (position < 0.25f) return kGreen;
  if (position < 0.50f) return kYellow;
  if (position < 0.75f) return kAmber;
  return kRed;
}

const char* linkLine(LinkState state) {
  switch (state) {
    case LinkState::DrivingPending:   return "Waiting for telemetry";
    case LinkState::Joining:          return "Joining Wi-Fi";
    case LinkState::Unresolved:       return "Can't find that PC name";
    case LinkState::Unreachable:      return "No signal from the PC";
    case LinkState::Stale:            return "Telemetry stopped";
    case LinkState::NoSim:            return "No sim running";
    case LinkState::UnsupportedTitle: return "Title not supported";
    case LinkState::AdapterFault:     return "Plugin fault - see SimHub log";
    case LinkState::VersionMismatch:  return "Version mismatch";
    case LinkState::Driving:          return "";   // the driving screen shows instead
  }
  return "";
}

void begin() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  tft.init();
  tft.setRotation(1);           // landscape, 320x240, fixed above the wheel
  tft.fillScreen(kBlack);       // pure black at rest: least light at night

  // Ground truth rather than assumption: what the library believes the usable area is after
  // rotation. If this disagrees with the panel, the driver's rotation mapping is the suspect.
  Serial.print("[panel] usable area after setRotation(1): ");
  Serial.print(tft.width());
  Serial.print("x");
  Serial.println(tft.height());
}

void drawOrientationTest() {
  const int w = tft.width(), h = tft.height();
  tft.fillScreen(kBlack);

  // A border traced at the library's claimed bounds. If any edge is missing or runs off, the
  // claimed area and the physical panel disagree.
  tft.drawRect(0, 0, w, h, kWhite);

  // Corner markers, each a different colour, so the report can be unambiguous about orientation
  // and about which edges are reachable.
  tft.fillRect(2, 2, 40, 24, kRed);                 // top-left
  tft.fillRect(w - 42, 2, 40, 24, kGreen);          // top-right
  tft.fillRect(2, h - 26, 40, 24, kAmber);          // bottom-left

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kWhite, kBlack);
  char dims[24];
  snprintf(dims, sizeof(dims), "%dx%d", w, h);
  tft.drawString(dims, w / 2, h / 2, 4);
}

void drawBoot(const char* deviceId, const char* firmwareVersion) {
  tft.fillScreen(kBlack);
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(kWhite, kBlack);
  tft.drawString("cyd-sim-dash", kWidth / 2, 78, 4);

  tft.setTextColor(kAmber, kBlack);
  tft.drawString(deviceId, kWidth / 2, 126, 4);

  tft.setTextColor(kGreen, kBlack);
  tft.drawString(firmwareVersion, kWidth / 2, 166, 2);
}

void drawPortal(const char* apName) {
  tft.fillScreen(kBlack);
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(kAmber, kBlack);
  tft.drawString("Setup needed", kWidth / 2, 70, 4);

  tft.setTextColor(kWhite, kBlack);
  tft.drawString("Join this Wi-Fi network:", kWidth / 2, 116, 2);

  tft.setTextColor(kGreen, kBlack);
  tft.drawString(apName ? apName : "", kWidth / 2, 148, 4);

  tft.setTextColor(kWhite, kBlack);
  tft.drawString("then follow the page that opens", kWidth / 2, 186, 2);
}

void drawMessage(const char* text) {
  tft.fillScreen(kBlack);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kWhite, kBlack);
  tft.drawString(text ? text : "", kWidth / 2, kHeight / 2, 4);
}

void drawLink(LinkState state) {
  tft.fillScreen(kBlack);
  tft.setTextDatum(MC_DATUM);

  // [SLICE 14] An icon belongs left of this line, one per condition, composed from primitives.
  // Until then the line carries the whole message, which is why every line reads as a complete
  // sentence rather than a label.
  tft.setTextColor(kWhite, kBlack);
  tft.drawString(linkLine(state), kWidth / 2, kHeight / 2, 4);
}

// ---------------------------------------------------------------------------
// SCREEN-DRIVING - the product. All four elements, composed.
// ---------------------------------------------------------------------------

uint32_t lastDrawUs() { return g_lastDrawUs; }
uint32_t worstDrawUs() { return g_worstDrawUs; }
uint32_t drawCount() { return g_drawCount; }

void invalidate() {
  g_shown.valid = false;
}

void drawDriving(const DisplayState& state, uint32_t nowMs) {
  const uint32_t drawStartedUs = micros();
  const bool flashPhase = (state.shift == ShiftPhase::Flashing) && flashOn(nowMs);

  // The background colour for this instant. Neutral and unavailable are both black: an unavailable
  // shift cue must look like no cue at all, never like a dim one, or it would read as information.
  uint16_t background = kBlack;
  switch (state.shift) {
    case ShiftPhase::Ramping:  background = rampColour(state.rampPosition); break;
    case ShiftPhase::Flashing: background = flashPhase ? kRed : kBlack;               break;
    case ShiftPhase::Neutral:
    case ShiftPhase::Unavailable:
    default:                   background = kBlack;                                  break;
  }

  const bool firstDraw   = !g_shown.valid;
  const bool bgChanged   = firstDraw || background != g_shown.background;
  const bool gearChanged = firstDraw ||
                           state.gearPresent != g_shown.gearPresent ||
                           strncmp(state.gearGlyph, g_shown.gearGlyph, sizeof(g_shown.gearGlyph)) != 0;
  const bool barsChanged = firstDraw ||
                           state.barLeft != g_shown.barLeft ||
                           state.barRight != g_shown.barRight;

  if (!bgChanged && !gearChanged && !barsChanged) return;   // a steady scene costs nothing

  // 1. The shift cue: two bands, top and bottom of the gear region.
  //
  // The whole background used to carry this. Measured on the device, a full-region repaint is
  // ~12 ms of bus time, and the panel refreshes about every 15 ms - near enough the same rate that
  // the write and the scan beat against each other and the boundary crawls across the glass as a
  // diagonal. Drawing less often did not help, because the problem is the length of one sweep
  // rather than how many there are.
  //
  // Two bands are about a third of the pixels, so a sweep is ~4 ms: comfortably shorter than a
  // refresh, which turns a crawling diagonal into a brief tear. Decided by the operator on
  // 2026-09-22 after seeing the alternatives; it amends U5 and CAP-SHIFT.
  //
  // It also reads more like a shift light than a full-field wash, and it leaves the centre
  // permanently black - which is why the glyph can now be larger, and why it is always legible
  // against one known colour rather than against the whole ramp.
  if (bgChanged) {
    tft.startWrite();
    tft.fillRect(kGearX, 0, kGearWidth, kBandHeight, background);
    tft.fillRect(kGearX, kHeight - kBandHeight, kGearWidth, kBandHeight, background);
    tft.endWrite();
  }

  // 2. The gear glyph, on steady black between the bands.
  //
  // Only repainted when the gear itself changes - the bands no longer touch this area, so a flash
  // phase costs nothing here. That is most of why the flash got cheaper.
  if (gearChanged) {
    const bool haveGlyph = state.gearPresent && state.gearGlyph[0] != '\0';

    tft.startWrite();
    tft.fillRect(kGearX, kBandHeight, kGearWidth, kHeight - 2 * kBandHeight, kBlack);
    if (haveGlyph) {
      // ONE proportional face for every gear in the domain. This replaces a pair of seven-segment
      // fonts and fixes three separate faults at once:
      //
      //   - Fonts 7 and 8 are different typefaces, so a one-character gear and a two-character one
      //     did not look like the same display. Switching between them mid-shift is exactly the
      //     kind of change that reads as a fault rather than as information.
      //
      //   - A seven-segment "1" lights only the two right-hand segments of its cell, which is
      //     correct for a real seven-segment display and wrong here: in "18" it leaves a gap on the
      //     left and the pair looks shoved against the right edge.
      //
      //   - **Neither font contains R or N.** TFT_eSPI's fonts 7 and 8 carry "1234567890:-." and
      //     nothing else, so reverse and neutral - two of the twenty values in the gear domain, and
      //     the two a driver most needs to be certain of - drew nothing at all. Confirmed on the
      //     glass by the operator on 2026-09-22.
      //
      // FreeSansBold has the full character set and proportional metrics, so R, N and every digit
      // render in one style and a "1" sits where a "1" should.
      tft.setFreeFont(&FreeSansBold24pt7b);
      tft.setTextColor(kWhite, kBlack);
      tft.setTextDatum(MC_DATUM);

      tft.setTextSize(gearTextSize());
      tft.drawString(state.gearGlyph, kGearX + kGearWidth / 2, kHeight / 2);

      tft.setTextSize(1);        // every other screen assumes the defaults
      tft.setFreeFont(nullptr);
    }
    tft.endWrite();
  }

  // 3. The edge bars, and only when they change.
  //
  // Drawing them last still holds the composition rule (U6): a lit bar stays solid white through
  // both phases of the flash and is never suppressed. With the bands confined to the gear region
  // the bars are now physically untouched by the shift cue, so the rule holds by geometry as well
  // as by ordering.
  if (barsChanged) {
    tft.startWrite();
    tft.fillRect(0, 0, kBarWidth, kHeight, state.barLeft ? kWhite : kBlack);
    tft.fillRect(kRightBarX, 0, kBarWidth, kHeight, state.barRight ? kWhite : kBlack);
    tft.endWrite();
  }

  g_shown.valid       = true;
  g_shown.background  = background;
  g_shown.gearPresent = state.gearPresent;
  strncpy(g_shown.gearGlyph, state.gearGlyph, sizeof(g_shown.gearGlyph) - 1);
  g_shown.gearGlyph[sizeof(g_shown.gearGlyph) - 1] = '\0';
  g_shown.barLeft     = state.barLeft;
  g_shown.barRight    = state.barRight;

  // How long the frame actually took to reach the glass.
  //
  // Measured rather than reasoned about, because the first attempt at this problem was reasoned
  // about and fixed the wrong thing. The operator sees a sweep; whether that sweep is bus time,
  // palette conversion, or glyph rendering decides which lever is worth pulling, and nothing short
  // of a number distinguishes them.
  g_drawCount++;
  const uint32_t elapsed = micros() - drawStartedUs;
  g_lastDrawUs = elapsed;
  if (elapsed > g_worstDrawUs) g_worstDrawUs = elapsed;
}

}  // namespace panel
}  // namespace cyd
