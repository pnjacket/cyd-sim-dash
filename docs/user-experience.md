---
artifact: product-doc
role: concern
concern-id: user-experience
behavior: module
trigger: interactive_ui
in-scope-subaspects: [navigation-ia-contract, screen-specifications, states, user-journeys]
current-rung: contract-grade
status: draft
version: 0.4.0
---

# User Experience — cyd-sim-dash

> Five screens, and the ambient states of the one that matters at 200 km/h.

## Purpose & Scope

Almost all of this product's value is in one screen the user never interacts with and only ever
glances at. That inverts the usual emphasis: navigation is nearly trivial, and the weight falls
entirely on **ambient state** — what the panel looks like at each instant as telemetry changes, and
how those looks compose when several fire at once.

This concern owns the screens, their layout geometry, their states, and the journeys across them.

## Non-goals / Out-of-scope

- **`component-reuse-design-system-binding`** — *absent*. A hand-rolled UI drawing primitives to a
  320×240 panel; no component library exists to bind to. What remains is the in-product visual
  consistency rule, carried by the screen specifications below. No re-entry note owed.
- **`responsive-layout`** — *deferred*. One fixed panel, one fixed orientation, known at compile
  time; every dimension below is an absolute pixel value. Recorded as deferred rather than absent
  because the subject does exist — other CYD variants have different panels. [FUTURE-SCOPE]
  Re-entry: replace the absolute pixel values here with a layout derived from panel dimensions
  reported at boot.
- **Accessibility is out of scope for v1.** The classic green–amber–red ramp was chosen knowingly.
  [FUTURE-SCOPE] Re-entry: a selectable colourblind-safe palette, and a measured contrast contract
  for the glyph. Dictum records an advisory scope warning for a UI product with accessibility
  fully out; it blocks nothing. **One safety-adjacent decision was nevertheless taken deliberately
  rather than by default** — the flash rate, see Design Decisions.
- **No touch interaction in v1** — *deferred*. The panel is read-only, so no panel screen has any
  interactive control. [FUTURE-SCOPE] Returns if touch is used.
- **Modal inertness is degenerate here.** The standard requires modal layers to be inert to both
  pointer and keyboard. No panel screen has layers, modals, pointer input or keyboard input, so
  there is nothing to make inert. Recorded because the rule was applied, not skipped.

## Requirements

### Navigation / IA contract

Five screens, minted in *Contracts*. Transitions are driven by device state rather than by user
action, with one exception: the configuration page, which a person opens deliberately.

Two of the five are served over HTTP and therefore have real routes; the three panel screens have
no URL surface and are specified by their transition conditions instead, which the standard permits
for routeless products.

**A previously open question is resolved:** *Connecting* and *Idle* are **one screen**, not two.
Both are conditions of the same link-state screen, which also covers unsupported title, adapter
fault and version mismatch. Nine conditions, one layout.

### Screen specifications

**The driving screen** is the one that matters, and its geometry is absolute:

| Region | Bounds | Content |
|---|---|---|
| Left bar | x 0–31, y 0–239 | 32 px wide, full height |
| Right bar | x 288–319, y 0–239 | 32 px wide, full height |
| Gear region | x 32–287, y 0–239 | 256 × 240 for the glyph |

The **background ramp fills the entire panel**, bar strips included. A lit bar paints **white** over
its strip; an unlit bar simply shows the ramp, so the binary is white-versus-whatever-the-ramp-is.
That is why bars are painted last and why their colour had to survive the whole palette.

The **gear glyph** is centred in the gear region, drawn **white with a black outline** thick enough
to read against any background the ramp produces, including mid-flash. It is auto-sized: a single
character fills the available height; two characters are sized to fit 256 px wide with margin.
Outline rather than luminance-switching, because a switch point visible mid-ramp would flip the
glyph at precisely the wrong moment.

The other four screens carry text and glyphs at ordinary sizes; they are read at rest, not at
speed.

### States

This is the heart of the concern. The driving screen's ambient states — every one of them driven by
an arriving frame rather than by a user action:

- **Neutral** — below the ramp start. Background **pure black**, which throws least light at night
  and makes the first green maximally noticeable.
- **Ramping** — between the two thresholds. Colour **interpolates continuously** green → amber →
  red across the window, so the background conveys roughly how far through you are.
- **Flashing** — above the flash threshold. Background alternates red and black at **3 Hz, 50 %
  duty**, continuing for as long as RPM stays there. No separate over-rev state; the flash does not
  time out.
- **Edge bars** — independently white or ramp-coloured. Both may be lit at once.
- **Gear** — any value in its domain, including neutral and reverse.
- **Element unavailable** — an element the running title cannot supply, arriving as `null`. In v1,
  with iRacing the only title, this should never occur in normal use, which makes it a useful
  **defect signal** rather than a normal condition. [FUTURE-SCOPE] In v2 it becomes an everyday
  state, and the presentation decided then should account for a dark bar also being the product's
  main failure symptom.

**Composition rule — the case that matters.** A lit bar and a red flash coincide during
wheel-to-wheel racing at the shift point, which is exactly when both signals matter most. The rule
is therefore: **bars are painted last, over both phases of the flash, and are never suppressed.**
Neither signal is diluted, and proximity remains readable through the flash. This is what ruled out
red, amber and green as bar colours.

### User journeys

Five journeys, minted in *Contracts*, covering first setup, an ordinary session, recovery from a
WiFi drop, reconfiguration after the PC's address changes, and a protocol version mismatch. The
last three are failure and change paths rather than happy paths, and they exist here because each
one traverses screens that would otherwise never be exercised.

## Open Questions

- [REVISIT] The nine link-state icons are specified as primitive compositions rather than drawn
  artwork. They should be judged together on the panel once drawn — nine glyphs that are each
  sensible alone can still be confusable as a set.
- [REVISIT] **Night brightness.** A full-screen ramp with no auto-dim will be bright in a dark
  room. Pure black at rest and a 3 Hz rather than faster flash both help; the night-comfort success
  criterion tests precisely this, and the light sensor remains unused by decision.
- [REVISIT] The exact ramp interpolation stops. Continuous blend is specified; whether green→amber
  and amber→red split the window evenly is a tuning matter best settled on the rig.

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Product & Requirements | each screen names the capabilities it realises, and each journey its persona |
| Domain & Data | the display-state entity supplies every value rendered here; its `linkState` domain is the nine conditions below |
| Interfaces & Contracts | the two HTTP screens' form fields, validation and empty-value semantics are owned there as data-entry contracts; the error catalogue fixes which condition reaches which screen |
| Architecture | the renderer component and the dirty-region strategy; `PATTERN-ERROR` decides hard-fault-to-screen versus soft-fault-to-counter |

## Examples / Worked scenarios

**Approaching the shift point with a car alongside.** RPM crosses the ramp start; the background
leaves black and begins blending green. A car draws level on the left and the left strip turns
white. RPM keeps climbing, the background reaching red as it nears the upper threshold. It crosses,
and the panel begins alternating red and black three times a second — with the left strip still
solid white through both phases, because bars are painted last. The gear glyph, white with its
black outline, stays readable throughout.

**Booting cold.** Power on. The panel shows the device identity and firmware version briefly, then
hands to the link-state screen showing *joining WiFi*, then *waiting for PC*. Frames arrive and it
switches to driving.

**Loading Assetto Corsa on a v1 device.** Frames keep arriving, with status `unsupportedTitle`. The
link-state screen shows the unsupported-title icon and a line naming the title. The device is
working correctly and says so, rather than going dark and looking broken.

**The PC is switched off mid-session.** Frames stop. For up to two seconds the panel continues
showing the last state, including a flash if RPM was above threshold. At two seconds it switches to
the link-state screen showing *no signal from PC*.

**Replacing the sim PC.** The new PC has a different address. The device still joins WiFi, so the
portal never raises; the panel shows *no signal from PC* indefinitely. The operator browses to the
device's address, authenticates, and updates the host. Note the limitation this journey exposes: the
panel names the condition but not the stale address, so this diagnosis requires the configuration
page — see Design Decisions.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| Flash at **3 Hz**, 50 % duty | Full-field flashing between roughly 3 and 60 Hz is the range associated with photosensitive seizures, and general guidance caps large-area flashing at three per second. The product may be shared publicly | Slightly less urgent than a faster flash. Taken deliberately rather than by default, despite accessibility being out of scope |
| White glyph with a **black outline** | Reads against every background the ramp produces with no conditional logic, and never needs revisiting when the palette changes | A heavier-looking glyph than a plain fill |
| Bars **32 px, full height, white** | Largest practical area and highest luminance, which is what peripheral vision actually catches; white survives green, amber, red and black alike | Bars can never be red, amber or green. More likely to draw the eye when you would rather it did not |
| Bars painted **last, never suppressed** | A car alongside at the shift point is the moment both signals matter most | The bar colour constraint above follows from this |
| ~~**Continuous blend** ramp~~ → **four discrete stops**, reversed 2026-09-22 | The original reasoning was that a blend conveys *how far* through the window you are rather than merely which band. Measured against real glass, the cost was not worth it: a blend repaints on every RPM step, each repaint is a visible sweep, and under acceleration the panel never settles. The operator judged the constant sweeping more distracting than the extra precision was useful | A stepped change is more noticeable peripherally than a smooth one — which the original decision counted as a drawback and is arguably an advantage for a shift cue. Precision between stages is lost, and nothing was using it |
| **Pure black** at rest | Least light at night, strongest contrast when green first appears | At a glance, an idling panel can look switched off — contradicted by the gear glyph |
| Link screen is **icon plus one line**; detail lives on the configuration page | Keeps the panel glanceable and uncluttered | **Narrows a success criterion.** The panel names the condition but not the configured address, so diagnosing a wrong-but-resolvable host needs the configuration page. Product & Requirements' link-diagnosable criterion carries this as a traced exception |
| **Boot screen** showing identity and firmware | Firmware version is exactly what you want when a version mismatch is the suspect, and it is visible before anything else can fail | One more screen, briefly delaying the first useful state |

## Contracts

### Screens

| ID | Screen | Route / transition | Guard | Realises | Drives | States | Controls |
|---|---|---|---|---|---|---|---|
| `SCREEN-BOOT` | Identity splash | Entered at power-on; leaves to `SCREEN-LINK` after **3 s** | none | — | — | one: identity and firmware version | none |
| `SCREEN-DRIVING` | The product | Entered when a frame with status `live` has arrived within the staleness threshold; leaves to `SCREEN-LINK` when that ceases | enabling condition: fresh live frame | `CAP-GEAR`, `CAP-SHIFT`, `CAP-SPOTTER-LEFT`, `CAP-SPOTTER-RIGHT` | consumes `EVT-FRAME` | neutral · ramping · flashing · each bar lit or not · gear across its domain · element unavailable | **none** — no touch in v1 |
| `SCREEN-LINK` | Link state | Entered whenever `SCREEN-DRIVING`'s condition is unmet; leaves to it when met | none | `CAP-LINKSTATE` | consumes `EVT-FRAME` status and the local link | nine, exactly the `linkState` domain owned by Domain & Data: `drivingPending` · `joining` · `unresolved` · `unreachable` · `stale` · `noSim` · `unsupportedTitle` · `adapterFault` · `versionMismatch` | none |
| `SCREEN-PORTAL` | Provisioning | HTTP `/` on the device's own access point. Raised when unprovisioned, when the network cannot be joined, or when stored configuration is unreadable | none — the access point is the boundary | `CAP-PROVISION` | `UIF-PORTAL` | form · validating · per-step verification progress · per-step failure · success | owned by `UIF-PORTAL` |
| `SCREEN-CONFIG` | Configuration page | HTTP `/` on the device's LAN address while connected. Opened deliberately | the device credential | `CAP-RECONFIG` | `UIF-CONFIG` | unauthenticated · form · soft-fault counters · erase confirmation · saved | owned by `UIF-CONFIG` |

Every panel screen has **zero interactive controls**, so the standard's default/empty-input-state
requirement falls entirely to the two HTTP screens, where it is owned by the data-entry contracts.

### Link-state presentation

Nine conditions, each an icon composed from **drawing primitives the display library already
provides** — no bitmaps, no icon font, nothing to embed or licence. Each is paired with a
plain-language line, because an adopter arrives knowing none of these glyphs.

| `linkState` | Icon, from primitives | Line |
|---|---|---|
| `drivingPending` | three filled WiFi arcs with a hollow dot below | Waiting for telemetry |
| `joining` | three WiFi arcs, outermost hollow | Joining Wi-Fi |
| `unresolved` | three WiFi arcs with a question mark over them | Can't find that PC name |
| `unreachable` | three filled WiFi arcs with an arrow crossed out | No signal from the PC |
| `stale` | three filled WiFi arcs with two pause bars | Telemetry stopped |
| `noSim` | an empty rounded rectangle, like a blank screen | No sim running |
| `unsupportedTitle` | a rounded rectangle with a diagonal slash | *(title)* not supported |
| `adapterFault` | a triangle with an exclamation mark | Plugin fault — see SimHub log |
| `versionMismatch` | two offset rectangles with a gap between them | Version mismatch — *(device)* vs *(plugin)* |

Two lines interpolate a value: the unsupported title names the running title, and the version
mismatch names both versions. Those are the two conditions where the condition alone does not tell
you what to do.

The first five share a WiFi motif deliberately — they are all *link* problems and differ only in
how far along the chain the failure is. The last four have distinct silhouettes because they are
not link problems at all: the link is fine and something beyond it is wrong.

### Journeys

| ID | Journey | Persona | Path |
|---|---|---|---|
| `JOURNEY-FIRST-SETUP` | Flashed device to working panel, unaided | `PERSONA-ADOPTER` | `SCREEN-BOOT` → `SCREEN-PORTAL` → verification sequence → `SCREEN-LINK` → `SCREEN-DRIVING` |
| `JOURNEY-SESSION` | An ordinary race | `PERSONA-OPERATOR` | `SCREEN-BOOT` → `SCREEN-LINK` (joining, then no sim) → `SCREEN-DRIVING` → `SCREEN-LINK` at session end |
| `JOURNEY-RECOVERY` | WiFi drops mid-session and returns | `PERSONA-OPERATOR` | `SCREEN-DRIVING` → `SCREEN-LINK` (stale, then joining) → `SCREEN-DRIVING`, with no reboot and no user action |
| `JOURNEY-RECONFIG` | The sim PC's address changes | `PERSONA-OPERATOR` | `SCREEN-LINK` (no signal) → `SCREEN-CONFIG` from another machine → `SCREEN-DRIVING` |
| `JOURNEY-VERSION-MISMATCH` | One half updated, the other not | `PERSONA-OPERATOR` | `SCREEN-DRIVING` → `SCREEN-LINK` (version mismatch, naming both versions) → resolved by updating the lagging half |

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| U1 | Measured on the panel, the bars occupy x 0–31 and x 288–319 at full height, and the glyph is centred in the remaining region | driving-screen geometry |
| U2 | The gear glyph is legible against black. **Simplified by the band amendment of 2026-09-22**: the glyph now sits on permanently black ground, so it no longer has to survive green, amber, red and both flash phases — it has exactly one background. The glyph is also sized against the space the bands leave, rather than at a fixed size | glyph legibility |
| U3 | Every gear in the domain, including `N`, `R` and a two-character value, renders within the gear region without clipping | auto-sizing |
| U4 | Sweeping RPM through the window steps the bands through **four discrete stops** — green, yellow, amber, red — at the quarter points, beginning at the ramp threshold and reaching red at the flash threshold. Each stop is clearly distinguishable from its neighbours.<br><br>**Amended 2026-09-22 by operator decision.** This previously required a *continuous* blend with no visible banding, and the banding is now the point. Every colour change is a full band repaint and therefore one visible sweep across the glass; a blend changes colour on every RPM step, so a hard acceleration meant the panel was permanently mid-sweep. Four stops cap that at four repaints across the whole ramp however fast the engine revs. The shade between two stages carried no information a driver could act on, and discrete stages are what a driver reads peripherally anyway | discrete ramp stops |
| U5 | Above the flash threshold **two bands, at the top and bottom of the centre region,** alternate red and black at 3 Hz ± 10 %, and continue while RPM is held there. **Measured from a slow-motion video capture at a known frame rate** — a phone at 240 fps is adequate and free — by counting frames between transitions, because ± 10 % at 3 Hz is not resolvable by eye.<br><br>**Amended 2026-09-22, by operator decision, against measurement.** This previously flashed the whole background. A full-region repaint is ~12 ms of bus time and the panel refreshes about every 15 ms, so the write and the scan beat against each other and the boundary crawled across the glass as a diagonal — which the operator reported as more distracting than the cue was useful. Drawing less often did not help, because the problem is the length of one sweep rather than the number of them. Two bands are about a third of the pixels: measured at **4.3 ms**, comfortably inside one refresh, which turns a crawling diagonal into a brief tear. The centre stays black, which is also what allows a larger glyph | flash rate, no time-out |
| U6 | With a bar lit during the flash, the bar remains solid white through both phases and is never suppressed | composition rule |
| U7 | Each bar lights and clears independently, and both are lit simultaneously when both sides report a car | `CAP-SPOTTER-LEFT`, `CAP-SPOTTER-RIGHT` |
| U8 | Each of the nine link conditions is provoked and each displays its specified icon and line; the two interpolating lines show the actual title and the actual version pair | `SCREEN-LINK` states |
| U13 | The nine icons are viewed together at panel size from the driving position and each is identifiable without reading its line | icon distinguishability as a set |
| U9 | Power-on shows device identity and firmware version before any connection attempt is reported | `SCREEN-BOOT` |
| U10 | Each of the five journeys is walked end to end, and every screen transition occurs without a reboot or a user action except where the journey specifies one | `JOURNEY-*` |
| U11 | The configuration page is unreachable without the credential, and the erase action requires confirmation | `SCREEN-CONFIG` guard |
| U12 | At rest below the ramp threshold the background is pure black | neutral state |

