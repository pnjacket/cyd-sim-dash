// TFT_eSPI configuration for the ESP32-2432S028R ("Cheap Yellow Display").
//
// ---------------------------------------------------------------------------
// This file exists to defuse the single most likely reason a CYD project does
// not work.
//
// TFT_eSPI normally takes its pin configuration from `User_Setup.h` *inside the library folder*.
// That means every adopter must edit a file in their libraries directory, the edit is silently
// lost on a library update, and getting it wrong produces a blank screen with no error of any
// kind. Integrations records it as the most likely cause of an adopter failing to get a working
// panel.
//
// TFT_eSPI's `User_Setup_Select.h` is guarded by `#if !defined(USER_SETUP_LOADED)`. So if we
// define everything here and include this header *before* `TFT_eSPI.h`, the library skips its own
// setup selection entirely and nothing in the libraries folder is ever touched.
//
// The one rule that keeps this working: **`TFT_eSPI.h` is included in exactly one translation
// unit (`panel.cpp`), and only after this header.** Including it anywhere else would pick up the
// library's own defaults and silently disagree with this configuration.
// ---------------------------------------------------------------------------

#ifndef CYD_TFT_CONFIG_H
#define CYD_TFT_CONFIG_H

#define USER_SETUP_LOADED

// Panel driver and colour polarity.
//
// Settled against real glass on 2026-09-21, and worth recording in full because the usual advice
// ("try the other driver") does not resolve it — the two variants differ in *both* rotation
// mapping and inversion, so neither alone is correct on this board:
//
//   ILI9341_2_DRIVER alone : orientation correct (landscape), colours inverted
//   ILI9341_DRIVER   alone : colours correct, orientation wrong — the library reported 320x240
//                            while the panel rendered portrait, i.e. setRotation never reached it
//
// The combination below is the one that works: the _2 driver for its rotation table, with
// inversion explicitly ON.
//
// The polarity flag is worth a note, because the name misleads. It is not "render normally" — it
// is a bit sent to the controller, and which setting looks right depends on the panel. With
// INVERSION_OFF this board rendered exact colour complements (red as cyan, green as magenta,
// amber as dark blue, black as white), so ON is correct here.
#define ILI9341_2_DRIVER
#define TFT_INVERSION_ON

// Native panel orientation is portrait; the product runs landscape via setRotation().
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// SPI wiring. The display and the touch controller share the bus and are arbitrated by separate
// chip selects; touch is unused in v1 and its CS is therefore left undriven.
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // tied to the module's own reset, not to a GPIO

#define TFT_BL   21   // backlight
#define TFT_BACKLIGHT_ON HIGH

// Fonts. Font 8 is the large seven-segment face the gear glyph is built from; the smaller ones
// carry the boot and link screens.
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// 80 MHz, not 55.
//
// The ESP32 derives the SPI clock as 80 MHz / N, so 55 MHz is not achievable: it rounds down to
// 40 MHz, which is what this board was actually running. That doubled every full-region repaint,
// and a repaint is what the operator sees sweeping across the glass during the shift flash.
//
// 80 MHz requires the IOMUX pins rather than the GPIO matrix, and this display is wired to exactly
// the HSPI IOMUX set - MISO 12, MOSI 13, SCLK 14, CS 15 - so it is available here. On a board wired
// differently this would have to come back down.
//
// If artefacts appear on the glass, this is the first thing to revert: it is one number, and the
// panel is the only place the effect is visible.
#define SPI_FREQUENCY       80000000
#define SPI_READ_FREQUENCY  20000000

#endif  // CYD_TFT_CONFIG_H
