// Panel renderer — the only place TFT_eSPI is touched.
//
// Realises COMPONENT-RENDER and owns the layout geometry User Experience fixes in absolute
// pixels. Nothing here decides *what* to show: it is handed a DisplayState and paints it.
//
// Slice 8 scope: panel bring-up, the boot screen, and a text-only link screen. The nine link
// icons and the driving screen land in later slices.

#ifndef CYD_PANEL_H
#define CYD_PANEL_H

#include "display_state.h"

namespace cyd {
namespace panel {

// Layout, from User Experience. Absolute because the panel is fixed: 320x240, landscape, mounted
// above the wheel. There is no responsive layout and these are not suggestions.
constexpr int kWidth      = 320;
constexpr int kHeight     = 240;
constexpr int kBarWidth   = 32;          // 32 px, full height, each extreme edge
constexpr int kLeftBarX   = 0;
constexpr int kRightBarX  = kWidth - kBarWidth;   // 288
constexpr int kGearX      = kBarWidth;            // 32
constexpr int kGearWidth  = kWidth - 2 * kBarWidth;  // 256

// Colours, RGB565. The ramp runs green -> amber -> red as a continuous blend; a lit bar is white,
// which is the only colour that survives every point of that ramp plus the flash and the black
// at rest.
// The shift cue is two bands at the top and bottom of the gear region rather than the whole
// background. 40 px each leaves 160 px of steady black in the middle for the gear.
//
// The reason is measured, not aesthetic: a full-region repaint is ~12 ms of bus time and the panel
// refreshes about every 15 ms, so the write and the scan beat against each other and the boundary
// crawls across the glass as a diagonal. A band repaint is ~4 ms, comfortably inside one refresh,
// which turns a crawling diagonal into a brief tear.
constexpr int kBandHeight = 40;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGreen = 0x0640;
constexpr uint16_t kYellow = 0xFFE0;   // the second ramp stop
constexpr uint16_t kAmber = 0xFD20;
constexpr uint16_t kRed   = 0xF800;

// Brings up the display and the backlight. Call once.
void begin();

// Bring-up diagnostic: a border at the claimed bounds, three coloured corner markers and the
// dimensions in the centre. Temporary - it exists to settle orientation against real glass, and
// comes out once slice 8's manual pass passes.
void drawOrientationTest();

// SCREEN-BOOT: device identity and firmware version, held briefly so the panel says what it is
// before anything can go wrong. Exactly what you want when a version mismatch is the suspect.
void drawBoot(const char* deviceId, const char* firmwareVersion);

// Shown while the provisioning access point is up. The docs record this as an open decision:
// SCREEN-PORTAL is HTTP, and none of SCREEN-LINK's nine conditions is true while unprovisioned, so
// the panel would otherwise have nothing honest to show at the exact moment an adopter most needs
// telling what to do. This is the answer.
void drawPortal(const char* apName);

// A single centred line. Used by OTA progress and anywhere a transient message is owed.
void drawMessage(const char* text);

// SCREEN-LINK, text only for now: the plain-language line for a condition.
// [SLICE 14] The nine icons and the interpolating lines land with the link-state slice.
void drawLink(LinkState state);

// The plain-language line for a link condition. Exposed so tests and the serial log can use the
// same strings the panel shows, rather than a second set that can drift.
const char* linkLine(LinkState state);

// Blend along the ramp: 0.0 -> green, 0.5 -> amber, 1.0 -> red.
uint16_t rampColour(float position);

// SCREEN-DRIVING: all four elements at once - gear glyph, background ramp or flash, and the two
// edge bars. This is the product.
//
// Composition rule (U6): the bars are drawn AFTER the background, so a lit bar stays solid white
// through both phases of the flash and is never suppressed. The background is the urgent cue and
// the bars are the spatial one; losing the spatial cue at the exact moment the urgent one fires
// would be the worst possible trade.
//
// Redraws only what changed. A full-screen repaint at 60 Hz over SPI would tear visibly and starve
// the receive path; the previous state is remembered so a steady scene costs nothing.
void drawDriving(const DisplayState& state, uint32_t nowMs);

// How long the last driving-screen draw took, and the worst since boot, in microseconds. Reported
// through API-STATE: the operator sees a sweep across the glass, and this is the only way to know
// whether that sweep is bus time or something the renderer is doing badly.
uint32_t lastDrawUs();
uint32_t worstDrawUs();
uint32_t drawCount();

// Forget what is on screen, so the next draw is unconditional. Called when leaving a screen, since
// the change-detection above would otherwise skip redrawing an identical state on return.
void invalidate();


}  // namespace panel
}  // namespace cyd

#endif  // CYD_PANEL_H
