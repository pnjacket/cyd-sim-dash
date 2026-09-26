// The one translation unit that includes TFT_eSPI.h — and it does so only after tft_config.h,
// which is what keeps the library's own User_Setup.h out of the picture entirely.
#include "tft_config.h"

#include <TFT_eSPI.h>

#include "panel.h"

#include <math.h>

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
  // Discrete stops rather than a continuous blend, and deliberately few of them.
  //
  // A blend changes colour on every RPM step, and every colour change is a full band repaint - one
  // visible sweep across the glass. Discrete stops cap the repaints across the entire ramp however
  // fast the engine is revving, and discrete stages are what a driver reads peripherally anyway.
  //
  // The stops below were retuned on the rig on 2026-09-23, against how the window actually behaves
  // in a car rather than how it looks on a bench. Three things came out of driving it:
  //
  //   - **The bottom of the window should be black.** The ramp window is narrow and sits near the
  //     top of the rev range, so in ordinary driving the revs are almost always inside it. Starting
  //     at green meant the bands were lit essentially all the time, and a cue that is always on is
  //     not a cue.
  //
  //   - **The red stop is gone.** It sat immediately below the flash, which is also red, so it
  //     added a colour change that conveyed nothing - the very next event was red again, flashing.
  //
  //   - **Each remaining stop is longer.** Fewer, wider stages mean fewer changes crossing the
  //     window, which is what made it feel busy: four changes crammed into the second or so it
  //     takes to cross 6130-6690 rpm.
  //
  // The thresholds are the tuning surface. If a stage still feels too short, widening it is one
  // number here.
  if (position < 0.20f) return kBlack;    // in the window, but nothing worth saying yet
  if (position < 0.47f) return kGreen;
  if (position < 0.73f) return kYellow;
  return kAmber;                          // the last stage before the flash takes over
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

void backlight(bool on) {
  digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
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

// ---------------------------------------------------------------------------
// SCREEN-LINK - the nine icons
// ---------------------------------------------------------------------------
//
// Composed from the drawing primitives TFT_eSPI already provides: arcs, circles, rectangles and
// lines. No bitmaps, no icon font, nothing to embed or licence. That was a deliberate constraint
// rather than an economy - an icon set as an asset is an asset to keep in step with the firmware, and
// nine glyphs at this size do not need one.
//
// Geometry is absolute, like the rest of this file. The panel is fixed and there is no responsive
// layout.
//
// The grouping is the substance and is worth stating where the code is: the first five share a Wi-Fi
// motif because they are all *link* problems differing only in how far along the chain the failure is,
// and the last four have deliberately distinct silhouettes because the link is fine and something
// beyond it is wrong. A glance should say which of those two worlds you are in before it says which
// condition.

namespace {

// Icon geometry. The fan's origin sits low so the whole glyph reads as radiating upward from a point,
// which is what makes it a Wi-Fi symbol rather than three stacked curves.
constexpr int kIconCx    = kWidth / 2;
constexpr int kIconOy    = 118;       // the fan's origin, and the dot's centre
constexpr int kLineY     = 178;       // the plain-language line, below every icon

// TFT_eSPI's arc angles: 0 is at six o'clock and they run clockwise, so twelve o'clock is 180. An
// upward-opening fan is therefore centred on 180.
constexpr int kFanFrom = 133;
constexpr int kFanTo   = 227;

// Radial position at an arc angle, in that same convention.
void radial(int cx, int cy, int r, int degrees, int& x, int& y) {
  const float a = static_cast<float>(degrees) * 3.14159265f / 180.0f;
  x = cx - static_cast<int>(static_cast<float>(r) * sinf(a));
  y = cy + static_cast<int>(static_cast<float>(r) * cosf(a));
}

// A solid band of arc.
void arcFilled(int cx, int cy, int rOuter, int rInner, uint16_t colour) {
  tft.drawArc(cx, cy, rOuter, rInner, kFanFrom, kFanTo, colour, kBlack, true);
}

// The same band as an outline: its two boundaries, closed at each end. This is what "hollow" means in
// the contract - not a thinner arc, which would read as a weaker signal rather than an absent one.
void arcHollow(int cx, int cy, int rOuter, int rInner, uint16_t colour) {
  tft.drawArc(cx, cy, rOuter, rOuter - 1, kFanFrom, kFanTo, colour, kBlack, true);
  tft.drawArc(cx, cy, rInner + 1, rInner, kFanFrom, kFanTo, colour, kBlack, true);

  int xo, yo, xi, yi;
  radial(cx, cy, rOuter, kFanFrom, xo, yo);
  radial(cx, cy, rInner, kFanFrom, xi, yi);
  tft.drawLine(xo, yo, xi, yi, colour);
  radial(cx, cy, rOuter, kFanTo, xo, yo);
  radial(cx, cy, rInner, kFanTo, xi, yi);
  tft.drawLine(xo, yo, xi, yi, colour);
}

// The shared Wi-Fi motif: three arcs and a dot. `hollowBand` names the one band drawn as an outline,
// counting 1 as the innermost, or 0 for none. `filledDot` distinguishes the two conditions that are
// otherwise identical.
void wifiFan(int hollowBand, bool filledDot) {
  const int radii[3][2] = {{24, 18}, {38, 32}, {52, 46}};
  for (int band = 0; band < 3; ++band) {
    if (band + 1 == hollowBand) {
      arcHollow(kIconCx, kIconOy, radii[band][0], radii[band][1], kWhite);
    } else {
      arcFilled(kIconCx, kIconOy, radii[band][0], radii[band][1], kWhite);
    }
  }
  if (filledDot) {
    tft.fillCircle(kIconCx, kIconOy, 7, kWhite);
  } else {
    tft.drawCircle(kIconCx, kIconOy, 7, kWhite);
    tft.drawCircle(kIconCx, kIconOy, 6, kWhite);
  }
}

// A screen outline, for the three conditions about what is running rather than about the link.
void screenRect(bool slashed) {
  const int w = 96, h = 68;
  const int x = kIconCx - w / 2, y = kIconOy - 58;
  tft.drawRoundRect(x, y, w, h, 8, kWhite);
  tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 7, kWhite);
  if (slashed) {
    // Corner to corner, thick enough to read at a glance from the driving position.
    for (int d = -1; d <= 1; ++d) {
      tft.drawLine(x + 8 + d, y + h - 8, x + w - 8 + d, y + 8, kWhite);
    }
  }
}

}  // namespace

// One icon, centred horizontally, above the line.
void drawLinkIcon(LinkState state) {
  switch (state) {
    // --- the five link conditions, sharing the Wi-Fi motif --------------------
    case LinkState::DrivingPending:
      // Everything is connected and the telemetry has not started: full signal, hollow dot. The dot
      // is the payload, so an outline there says the link is made and nothing is coming through it.
      wifiFan(/*hollowBand=*/0, /*filledDot=*/false);
      break;

    case LinkState::Joining:
      // Still associating: the outermost band is hollow, the conventional reading of a signal that is
      // not established yet.
      wifiFan(/*hollowBand=*/3, /*filledDot=*/true);
      break;

    case LinkState::Unresolved: {
      // Full signal with a question mark over it: the network is fine and the *name* is the problem.
      wifiFan(/*hollowBand=*/0, /*filledDot=*/true);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(kWhite, kBlack);
      tft.drawString("?", kIconCx, kIconOy - 34, 6);
      break;
    }

    case LinkState::Unreachable: {
      // Full signal with the path crossed out. The arrow is the thing struck through rather than the
      // signal, because the signal is genuinely fine - nothing is coming back.
      wifiFan(/*hollowBand=*/0, /*filledDot=*/true);
      const int y = kIconOy - 30;
      tft.fillRect(kIconCx - 26, y - 2, 52, 5, kBlack);      // clear a channel for the arrow
      tft.drawLine(kIconCx - 24, y, kIconCx + 22, y, kWhite);
      tft.drawLine(kIconCx + 22, y, kIconCx + 14, y - 7, kWhite);
      tft.drawLine(kIconCx + 22, y, kIconCx + 14, y + 7, kWhite);
      for (int d = -1; d <= 1; ++d) {                         // struck through, thickly
        tft.drawLine(kIconCx - 14 + d, y + 16, kIconCx + 14 + d, y - 16, kWhite);
      }
      break;
    }

    case LinkState::Stale:
      // Full signal, paused. Frames were arriving and stopped, which is exactly what a pause means and
      // is not the same fact as no signal at all.
      wifiFan(/*hollowBand=*/0, /*filledDot=*/true);
      tft.fillRect(kIconCx - 14, kIconOy - 42, 9, 26, kBlack);
      tft.fillRect(kIconCx + 5, kIconOy - 42, 9, 26, kBlack);
      tft.fillRect(kIconCx - 13, kIconOy - 41, 7, 24, kWhite);
      tft.fillRect(kIconCx + 6, kIconOy - 41, 7, 24, kWhite);
      break;

    // --- the four that are not link problems ---------------------------------
    case LinkState::NoSim:
      // A blank screen: the plugin is talking to us and there is nothing running behind it.
      screenRect(/*slashed=*/false);
      break;

    case LinkState::UnsupportedTitle:
      // A screen with something on it that we cannot use.
      screenRect(/*slashed=*/true);
      break;

    case LinkState::AdapterFault: {
      // The only warning triangle in the set, and the only icon that means "something broke" rather
      // than "something is absent". Kept white rather than amber: amber and red belong to the shift
      // cue, and a second meaning for them on another screen is how a colour stops being a signal.
      const int cy = kIconOy - 24, half = 52, h = 64;
      for (int d = 0; d < 2; ++d) {
        tft.drawLine(kIconCx, cy - h / 2 - d, kIconCx - half, cy + h / 2 - d, kWhite);
        tft.drawLine(kIconCx, cy - h / 2 - d, kIconCx + half, cy + h / 2 - d, kWhite);
        tft.drawLine(kIconCx - half, cy + h / 2 - d, kIconCx + half, cy + h / 2 - d, kWhite);
      }
      tft.fillRect(kIconCx - 2, cy - 14, 5, 24, kWhite);
      tft.fillRect(kIconCx - 2, cy + 16, 5, 5, kWhite);
      break;
    }

    case LinkState::VersionMismatch: {
      // Two offset blocks with a gap: two halves that no longer meet. The gap is the message, so it is
      // wide enough to survive being looked at quickly.
      const int w = 54, h = 44, gap = 16;
      const int cy = kIconOy - 26;
      tft.drawRect(kIconCx - gap / 2 - w, cy - h / 2 - 10, w, h, kWhite);
      tft.drawRect(kIconCx - gap / 2 - w + 1, cy - h / 2 - 9, w - 2, h - 2, kWhite);
      tft.drawRect(kIconCx + gap / 2, cy - h / 2 + 10, w, h, kWhite);
      tft.drawRect(kIconCx + gap / 2 + 1, cy - h / 2 + 11, w - 2, h - 2, kWhite);
      break;
    }

    case LinkState::Driving:
      break;   // no icon: the driving screen shows instead
  }
}

void drawLink(LinkState state, const LinkText& text) {
  tft.fillScreen(kBlack);

  drawLinkIcon(state);

  char line[64];
  linkLineInto(line, sizeof(line), state, text);

  // Font 2 for the line rather than 4. The interpolating lines are the longest in the set - a title
  // name or two version pairs - and a face that fits "Version mismatch" and clips the version numbers
  // would hide exactly the part that makes the line worth reading.
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(kWhite, kBlack);
  tft.drawString(line, kWidth / 2, kLineY, 4);

  // If the chosen face overruns the panel, drop to the smaller one rather than clipping. Checked
  // rather than assumed because the title comes off the wire and its length is not ours to choose.
  if (tft.textWidth(line, 4) > kWidth - 8) {
    tft.fillRect(0, kLineY - 16, kWidth, 32, kBlack);
    tft.drawString(line, kWidth / 2, kLineY, 2);
  }
}

// Offering a wake. The unreachable line stays exactly where it was, and a second line appears below
// it. Keeping the first line put is deliberate: the operator has just touched a dark panel, and a
// screen that rearranges itself makes them re-read the part that has not changed.
//
// Amber for the invitation, so it reads as an action rather than as more status. Red is the flash's
// and white is every other line's.
void drawWakeOffer() {
  tft.fillScreen(kBlack);
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(kWhite, kBlack);
  tft.drawString(linkLine(LinkState::Unreachable), kWidth / 2, kHeight / 2 - 24, 4);

  tft.setTextColor(kAmber, kBlack);
  tft.drawString("Touch again to wake the PC", kWidth / 2, kHeight / 2 + 26, 2);
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
