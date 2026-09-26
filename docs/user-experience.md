---
artifact: product-doc
role: concern
concern-id: user-experience
behavior: module
trigger: interactive_ui
in-scope-subaspects: [navigation-ia-contract, screen-specifications, states, user-journeys]
current-rung: contract-grade
status: published
version: 0.9.0
---

# User Experience — cyd-sim-dash

> Seven screens, and the ambient states of the one that matters at 200 km/h.

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
- **Accessibility is out of scope for v1.** The classic green–yellow–amber ramp, with red reserved
  for the flash, was chosen knowingly.
  [FUTURE-SCOPE] Re-entry: a selectable colourblind-safe palette, and a measured contrast contract
  for the glyph. Dictum records an advisory scope warning for a UI product with accessibility
  fully out; it blocks nothing. **One safety-adjacent decision was nevertheless taken deliberately
  rather than by default** — the flash rate, see Design Decisions.
- ~~**No touch interaction in v1** — deferred.~~ **In scope from 2026-09-26** for exactly one action:
  `CAP-WAKE-RIG`. The panel remains read-only in every other respect — one control, in one state,
  doing one thing. The default-and-empty-input-state requirement still falls to the two HTTP screens,
  because a touch target has no empty state to render.
- **Modal inertness is degenerate here.** The standard requires modal layers to be inert to both
  pointer and keyboard. No panel screen has layers, modals, pointer input or keyboard input, so
  there is nothing to make inert. Recorded because the rule was applied, not skipped.

## Requirements

### Navigation / IA contract

Seven screens, minted in *Contracts*. Transitions are driven by device state rather than by user
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

The **shift cue occupies two bands**, each `kBandHeight` = 40 px, at the top and bottom of the gear
region only. It never reaches the bar strips. The centre of the gear region — 256 x 160 — is
**permanently black**.

A lit bar paints **white** over its strip; an unlit bar is **black**. Because the bands are confined
to the gear region, a bar is now untouched by the shift cue as a matter of geometry rather than of
draw order — though bars are still painted last, so the rule holds twice over.

The **gear glyph** is centred in the black area between the bands, drawn plain **white on black**.
No outline: it was specified when the ramp filled the whole panel and the glyph had to survive every
colour the ramp could produce. With the centre permanently black the glyph has exactly one
background, and an outline would be decoration.

It is **sized once**, against the widest value the gear domain contains, and every gear then renders
at that size. Sizing per-glyph would make a one-character gear larger than a two-character one, so
the display would change size as well as content while shifting.

`SCREEN-SETUP` and `SCREEN-UPDATE` were **added 2026-09-23**, documenting behaviour that had been
built without a doc pass. `SCREEN-SETUP` answers a question this concern had left open: `SCREEN-PORTAL`
is an HTTP form, and none of `SCREEN-LINK`'s nine conditions is true while the device is
unprovisioned — so the glass had nothing honest to show at exactly the moment an adopter most needs
telling what to do. `SCREEN-UPDATE` exists because an update takes the panel out of service for
several seconds and a blank screen during it reads as a failure.

**Amended 2026-09-22/23** from a full-panel ramp with an outlined glyph. The reasons are recorded at
`U4`, `U5` and `CAP-SHIFT`; in short, a full-panel repaint is ~12 ms against a ~15 ms panel refresh
and tore visibly, and the lower part of a narrow window is better unlit than lit.

The other four screens carry text and glyphs at ordinary sizes; they are read at rest, not at
speed.

### States

This is the heart of the concern. The driving screen's ambient states — every one of them driven by
an arriving frame rather than by a user action:

- **Neutral** — below the ramp start. Background **pure black**, which throws least light at night
  and makes the first green maximally noticeable.
- **Ramping** — between the two thresholds. The bands step through **four discrete stops**: unlit for
  the lowest fifth of the window, then green, yellow and amber. Discrete rather than blended because
  every colour change is a band repaint and therefore a visible sweep; unlit at the bottom because
  the window is narrow and sits near the top of the rev range, so a lit bottom stage meant the bands
  were on almost continuously.
- **Flashing** — above the flash threshold. The **bands** alternate red and black at **3 Hz, 50 %
  duty**, continuing for as long as RPM stays there. The centre does not change. No separate
  over-rev state; the flash does not time out.
- **Edge bars** — independently white or black. Both may be lit at once.
- **Gear** — any value in its domain, including neutral and reverse.
- **Offering a wake** — reached from `unreachable` by a touch. The panel lights, keeps the
  `unreachable` line, and adds a second line inviting another touch. A second touch within the offer
  window sends the packet; the window lapsing returns the panel to `unreachable` and, if the blanking
  period has passed, to dark.

  **The offer window is ten seconds**, decided 2026-09-26. A judgement rather than a measurement, and
  recorded here so it is not read as one: the operator has just touched a panel that was dark, has to
  read a line that was not there a moment ago, and then decide. Two seconds would make the second
  touch a reflex test. Much longer and the panel sits armed after whoever touched it has walked away,
  which is the state the two-touch rule exists to avoid.

  **The first touch never sends.** A blanked panel cannot show what a touch is about to do, so the
  first one only makes the offer legible. That costs a second touch every time and buys immunity from
  a sleeve brushing the glass — a spurious wake is not catastrophic, but a panel that does things
  when nudged is one nobody trusts.

- **Waking** — entered when the packet is sent. The panel confirms briefly, then returns to the link
  screen and lets the normal ladder speak. It does **not** hold a "waking" message while the rig
  boots: the panel has no way to know whether the rig is coming up, and a message it cannot retract
  would keep asserting something it does not know. If the wake worked, the driving screen arrives on
  its own; if it did not, the operator is looking at `unreachable` again, which is the truth.

- **Blanked** — the backlight is off. Entered when the driving screen has not been showing for
  `blankAfterMinutes`, which covers every non-driving condition: `unreachable`, `noSim`, `stale`,
  `adapterFault`, `unresolved`, `joining` and `drivingPending`. Left the instant a live frame is
  accepted. Realises `CAP-BLANK`.

  Two exceptions, both messages a human is meant to read: **`SCREEN-UPDATE`** stays lit, because a
  dark panel mid-transfer reads as a crash and invites pulling the power — the one action that can
  actually brick the device; and **`versionMismatch`** stays lit, because naming the two versions so
  somebody updates the lagging half is the entire purpose of that condition.

  **A touch lights the panel regardless of state**, and restarts the blanking period. That is the one
  way the backlight comes on other than a live frame, and it is what makes `CAP-WAKE-RIG` reachable at
  all — the action lives in `unreachable`, which is precisely a state that blanks.

  **`SCREEN-SETUP` needs no exception.** It is drawn from the provisioning callback while
  `autoConnect` blocks inside setup, so the loop carrying the blanking timer is not running. It is
  exempt by construction, which is worth stating so it does not later look like an oversight.

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
- [REVISIT] **Night brightness.** Two things have narrowed this. The ramp is two 40 px bands rather
  than the full screen, so the lit area is roughly a third of what the concern was written about; and
  `CAP-BLANK` now darkens the panel entirely when it is not driving. What remains is brightness
  *while actually driving*, which neither change touches. Still no auto-dim. Pure black at rest and a 3 Hz rather than faster flash both help; the night-comfort success
  criterion tests precisely this, and the light sensor remains unused by decision.
- ~~The exact ramp interpolation stops.~~ **Settled on the rig, 2026-09-23.** There is no
  interpolation: four discrete stops at 0.20 / 0.47 / 0.73 of the window, the lowest unlit. The
  question of where the stops fall was indeed a tuning matter and was tuned by driving it.

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Product & Requirements | each screen names the capabilities it realises, and each journey its persona |
| Domain & Data | the display-state entity supplies every value rendered here; its `linkState` domain is the nine conditions below |
| Interfaces & Contracts | the two HTTP screens' form fields, validation and empty-value semantics are owned there as data-entry contracts; the error catalogue fixes which condition reaches which screen |
| Architecture | the renderer component and the dirty-region strategy; `PATTERN-ERROR` decides hard-fault-to-screen versus soft-fault-to-counter |

## Examples / Worked scenarios

**Approaching the shift point with a car alongside.** RPM crosses the ramp start; nothing changes
yet, because the lowest fifth of the window is unlit. A fifth of the way in the bands turn green. A
car draws level on the left and the left strip turns white. RPM keeps climbing and the bands step to
yellow, then amber. It crosses the upper threshold and the bands begin alternating red and black
three times a second — with the left strip still solid white through both phases, and the centre
black throughout. The gear glyph, white on that black centre, stays readable the whole way.

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
| ~~White glyph with a **black outline**~~ → **plain white on black**, reversed 2026-09-23 | The outline existed so the glyph would read against every colour a full-panel ramp could produce. The shift cue is now two bands that never touch the centre, so the glyph has exactly one background and the outline solved a problem that no longer exists | Nothing, as built. If the centre were ever to carry colour again the outline would have to come back, and this row is the reason why |
| Bars **32 px, full height, white** | Largest practical area and highest luminance, which is what peripheral vision actually catches; white survives green, amber, red and black alike | Bars can never be red, amber or green. More likely to draw the eye when you would rather it did not |
| Bars painted **last, never suppressed** | A car alongside at the shift point is the moment both signals matter most | The bar colour constraint above follows from this |
| ~~**Continuous blend** ramp~~ → **four discrete stops**, reversed 2026-09-22 | The original reasoning was that a blend conveys *how far* through the window you are rather than merely which band. Measured against real glass, the cost was not worth it: a blend repaints on every RPM step, each repaint is a visible sweep, and under acceleration the panel never settles. The operator judged the constant sweeping more distracting than the extra precision was useful | A stepped change is more noticeable peripherally than a smooth one — which the original decision counted as a drawback and is arguably an advantage for a shift cue. Precision between stages is lost, and nothing was using it |
| **Retuned again on the rig**, 2026-09-23: lowest stop unlit, red stop removed, remaining stops widened | Driving it showed what a bench could not. The window is narrow and sits near the top of the rev range, so ordinary driving is almost always inside it — a lit bottom stage meant the bands were on continuously, and a cue that is always on is not a cue. The red stop sat immediately below a flash that is also red, so it announced a change and then announced the same colour again | The bands say nothing at all for the lowest fifth of the window. That is deliberate, and it means the cue starts later than the shift-point arithmetic alone would suggest |
| **Pure black** at rest | Least light at night, strongest contrast when green first appears | At a glance, an idling panel can look switched off — contradicted by the gear glyph |
| **Blank the backlight when idle** rather than dim it, decided 2026-09-23 | The idling case is most of the time a rig is powered on, and a dark panel is worth more at night than a dim one. Dimming was available — GPIO 21 is PWM-capable — and was not chosen | A dark panel is indistinguishable from a dead one. Accepted knowingly: a panel that fails to wake is an obvious bug rather than a subtle one, and `API-STATE` reports `backlightOn` while the glass is dark. The cost is real and is not hidden |
| **Two touches to wake the rig**, decided 2026-09-26 | The first touch lights a dark panel and makes the offer readable; the second acts. A single touch from dark would act on a panel that could not show what it was about to do | A second touch every time, including when the operator knows exactly what they want. Accepted: the panel is read-only in every other respect, so an action that can fire from a brush against the glass is out of character for the whole device |
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
| `SCREEN-SETUP` | Panel-side provisioning | Shown on the glass whenever the provisioning access point is raised — unprovisioned, unable to join, or stored configuration unreadable | none | `CAP-PROVISION` | none; it renders local state | one: the access-point name and what to do with it | none |
| `SCREEN-UPDATE` | Firmware update in progress | Shown from the moment an over-the-air update begins transferring until the device reboots | none | none — it serves `ADR-ARDUINO-OTA` | none | one | none |

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
| U4 | Sweeping RPM through the window steps the bands through **black, green, yellow, amber** — the lower fifth of the window unlit, then three stages — beginning at the ramp threshold and handing over to the flash at the upper one. Each stage is clearly distinguishable from its neighbours.<br><br>**Amended twice by operator decision.** 2026-09-22 replaced a continuous blend with discrete stops: every colour change is a full band repaint and therefore a visible sweep, so a blend meant the panel was permanently mid-sweep under acceleration. 2026-09-23 retuned the stops *after driving it*, which produced three changes no bench test would have found. The bottom of the window is now **unlit**, because the window is narrow and sits near the top of the rev range — ordinary driving is almost always inside it, so a lit bottom stage meant the bands were on continuously, and a cue that is always on is not a cue. The **red stop is removed**, because it sat immediately below a flash that is also red and so announced nothing. And the remaining stages are **wider**: four changes crossing the window in about a second read as busy | discrete ramp stops |
| U5 | Above the flash threshold **two bands, at the top and bottom of the centre region,** alternate red and black at 3 Hz ± 10 %, and continue while RPM is held there. ~~Measured from a slow-motion video capture at a known frame rate.~~ **Waived by the operator 2026-09-23**, who judged the rate acceptable by eye and declined the instrumented measurement. The ± 10 % tolerance is therefore **not verified** — what is established is that the rate is acceptable to the person it exists for, which is the criterion that actually matters here. The cadence is derived from the clock rather than toggled, so drift is a property the arithmetic rules out and the unit tier asserts; the measurement would only have confirmed the panel matches the arithmetic.<br><br>**Amended 2026-09-22, by operator decision, against measurement.** This previously flashed the whole background. A full-region repaint is ~12 ms of bus time and the panel refreshes about every 15 ms, so the write and the scan beat against each other and the boundary crawled across the glass as a diagonal — which the operator reported as more distracting than the cue was useful. Drawing less often did not help, because the problem is the length of one sweep rather than the number of them. Two bands are about a third of the pixels: measured at **4.3 ms**, comfortably inside one refresh, which turns a crawling diagonal into a brief tear. The centre stays black, which is also what allows a larger glyph | flash rate, no time-out |
| U6 | With a bar lit during the flash, the bar remains solid white through both phases and is never suppressed | composition rule |
| U7 | Each bar lights and clears independently, and both are lit simultaneously when both sides report a car | `CAP-SPOTTER-LEFT`, `CAP-SPOTTER-RIGHT` |
| U8 | Each of the nine link conditions is provoked and each displays its specified icon and line; the two interpolating lines show the actual title and the actual version pair | `SCREEN-LINK` states |
| U13 | The nine icons are viewed together at panel size from the driving position and each is identifiable without reading its line | icon distinguishability as a set |
| U9 | Power-on shows device identity and firmware version before any connection attempt is reported | `SCREEN-BOOT` |
| U10 | Each of the five journeys is walked end to end, and every screen transition occurs without a reboot or a user action except where the journey specifies one | `JOURNEY-*` |
| U11 | The configuration page is unreachable without the credential **once one is set**, and the erase action requires confirmation. On a device whose credential is still empty the page is reachable and refuses to save until one is supplied — the recovery exception in `SEC-CREDENTIAL-POLICY`. Both paths are exercised | reconfiguration is gated |
| U12 | At rest below the ramp threshold the background is pure black | neutral state |

