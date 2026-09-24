# Implementation plan and work-item record

The **in-repo tracker** that Delivery Process names: there is no external tracker, and this file is
it. One row per slice, each referencing the contract IDs it touches. An item **references**
contract IDs; it never restates what a contract says. If a slice's description and a contract
disagree, the contract wins and the item is wrong.

Slice numbers are plan-local ordinals, **not contracts** — slices mint no IDs.

Derived from the implementation-planner pass of 2026-09-21 and reconciled against the build
playbook in [`docs/delivery-process.md`](docs/delivery-process.md), which is the normative order.

**Status key:** ☐ not started · ◐ in progress · ✅ done (DoD met) · ⛔ blocked

## Rig dependency

Three sittings need the rig, and they are the critical path. Everything else runs ahead of them.

| Sitting | Slice | What it needs |
|---|---|---|
| ~~Mapping~~ | ~~1~~ | **Done 2026-09-22.** One optional follow-up: a car on each side at once, to settle the both-sides spotter case |
| Lap capture | 6 | A recorded lap through the publisher |
| Live integration | 19 | The whole chain, end to end |

## Slices

| # | Slice | Type | Realises / touches | Rig? | Status |
|---|---|---|---|---|---|
| 1 | Confirm the iRacing property mapping | verification-only | `DEP-IRACING`, `DEP-SIMHUB`; closes Integrations' unconfirmed rows | **yes** | ◐ captured 2026-09-22; one row open (both-sides spotter) |
| 2 | Repository, CI and the records | cross-cutting | `POLICY-*`, `X8`, `V10`, `S12`; creates this file, the build-status record, `bindings.yaml`, notices, provenance | no | ✅ |
| 3 | Wire contract and shared fixtures | headless | `EVT-FRAME`, `EVT-REGISTRATION`, `INV-RAMP-ORDER`, `INV-RPM-NONNEG`, `INV-GEAR-DOMAIN`, `INV-STATUS-CONSISTENT` | no | ✅ |
| 4 | Display-state engine, whole | headless | `COMPONENT-STATE`, `ENTITY-DISPLAYSTATE`, `INV-FRESH-RENDER`, `INV-STAMP-ORDER`, the `linkState` ladder | no | ✅ |
| 5 | Plugin core, adapter interface, iRacing adapter, conformance suite | headless | `COMPONENT-PLUGIN-CORE`, `COMPONENT-ADAPTER-IRACING`, `CONFORMANCE-ADAPTER`, `ADR-ADAPTER-MODULES` | no | ✅ |
| 6 | Publisher, device table, capture writer | headless | `COMPONENT-PUBLISHER`, `ENTITY-CAPTURE`, `OUT-CAPTURE`, `INV-CAPTURE-CLEAN` | only for a *real lap* capture | ✅ |
| 7 | Replay tool and synthetic fixture generator | headless | `COMPONENT-REPLAY`, `ADR-REPLAY-STANDALONE` | no | ✅ |
| 8 | Device bring-up: panel and boot screen | full | `COMPONENT-RENDER`, `SCREEN-BOOT`, `DEP-TFT-ESPI` | no | ✅ manual pass done 2026-09-21 |
| 9a | WiFi provisioning, config store, captive portal | full | `COMPONENT-CONFIG`, `COMPONENT-WEB` (portal only), `ENTITY-DEVICECONFIG`, `UIF-PORTAL` | no | ◐ |
| 16a | OTA — **pulled forward** | full | `ADR-ARDUINO-OTA` | no | ✅ |
| 9 | Configuration store and provisioning portal | full | `COMPONENT-CONFIG`, `COMPONENT-WEB`, `ENTITY-DEVICECONFIG`, `UIF-PORTAL`, `SCREEN-PORTAL`, `CAP-PROVISION`, `INV-CONFIG-SINGLETON`, `INV-CONFIG-MIGRATION` | no | ☐ |
| 10 | Receive path, registration, cross-core handoff, `API-STATE`, E2E harness | full | `COMPONENT-NET`, `API-STATE`, `PATTERN-STATE-HANDOFF`, `PATTERN-RECONNECT`, `SEC-INPUT-BOUND`, `SEC-PARSER-FLOOR` | **for the manual pass** | ◐ code + host tests done; device was off |
| 11 | Gear | full | `CAP-GEAR`, `SCREEN-DRIVING` | manual pass | ◐ code done |
| 12 | Shift ramp and flash | full | `CAP-SHIFT` | manual pass | ◐ code done |
| 13 | Edge bars and the composition rule | full | `CAP-SPOTTER-LEFT`, `CAP-SPOTTER-RIGHT` | manual pass | ◐ code done |
| 14 | Link-state screen, nine conditions | full | `CAP-LINKSTATE`, `SCREEN-LINK` | manual pass | ◐ text done, icons owed |
| 15 | Configuration page, reconfiguration, erase | full | `CAP-RECONFIG`, `SCREEN-CONFIG`, `UIF-CONFIG`, `SEC-ERASE-OVERWRITE`, the four soft-fault counters | verification | ◐ code done |
| 16 | OTA and the update window | full | `SEC-OTA-WINDOW`, `SEC-OTA-IMAGE`, `ADR-ARDUINO-OTA` | no | ☐ |
| 17 | Security and error-catalogue verification | verification-only | every `ERR-*`, `PATTERN-ERROR`, `SEC-*` battery, `Q8`, `S1`–`S12` | no | ☐ |
| 18 | `E2E-STANDARD` conformance and the five journeys | verification-only | `E2E-STANDARD`, every `JOURNEY-*` | no | ☐ |
| 19 | Live iRacing integration | full | `CAP-PUBLISH`, `A8`, `X1` | **yes** | ◐ plugin packaged for the rig 2026-09-23 |
| 20 | Documentation, notice, provenance pass, v1 release | cross-cutting | `G1`, `G3`, `G5`, `G7`, `V5`, `V6` | no | ☐ |
| 21 | Adopter unaided-setup trial | verification-only | `SUCCESS-SETUP-UNAIDED`, `A10` | no | ☐ |
| 22 | Drive it and record the criteria | verification-only | every `SUCCESS-*`, `A1` | **yes** | ☐ |
| 23 | Backlight blanking | full | `CAP-BLANK`, `INV-BLANK-BOUND`, `ENTITY-DEVICECONFIG` schema 2, `UIF-CONFIG`, `API-STATE` | **feasibility test first** | ☐ blocked |
| — | **v2 Go/No-Go gate** | decision | reserved to the operator | — | ☐ |

## The mapping sitting is done with a tool, not a picker

The plan originally had this sitting done by hand through SimHub's property picker. It is done with
a **probe plugin** instead (`plugin/probe/`), for two reasons that only became clear once the rig
was in reach.

The first is a constraint the operator set: the sim PC has no source access, so anything that runs
there has to arrive as a compiled binary with instructions. A picker session would also have meant
transcribing values by hand, which is exactly the kind of evidence that cannot be re-checked later.

The second is that the picker cannot answer the question that matters most. Whether
`SpotterCarLeft` *counts* cars or merely *flags* presence is not visible in a static reading — it
needs the distinct values observed across a session where two cars are alongside at once. The probe
accumulates them; a person watching a number change cannot reliably do so.

What comes back is a property inventory with live values, a ~10 Hz time series, and a summary of
the distinct spotter values. That is the evidence `X1` asks for, in a form that can be replayed
against the adapter later rather than trusted once.

The probe is **throwaway**: it realises no contract, binds to no ID, and is deleted once the
mapping table is confirmed. It is recorded here so that it is not mistaken for product code.

## Why 9a and 16a were pulled forward

Every USB flash on this board needs a manual unplug / hold `BOOT` / replug, because its auto-reset
does not drive IO0. That made each iteration expensive enough to reorder the playbook: WiFi and OTA
went into the *second* flash so it became the last one. Everything since has gone over the air.

This is a deliberate, recorded deviation from the build playbook's order, not a drift. The slices
are still the same slices; only their sequence moved, and the reason is a property of this
hardware rather than a change of mind.

## Panel configuration, settled against real glass

Four OTA iterations, because the usual advice ("try the other ILI9341 driver") does not resolve it
on this board. The two variants differ in **both** rotation mapping and colour polarity:

| Configuration | Orientation | Colour |
|---|---|---|
| `ILI9341_DRIVER` | **wrong** — library reported 320x240 while the panel rendered portrait | correct |
| `ILI9341_2_DRIVER` | correct landscape | inverted |
| `ILI9341_2_DRIVER` + `TFT_INVERSION_OFF` | correct | still inverted — exact complements |
| **`ILI9341_2_DRIVER` + `TFT_INVERSION_ON`** | **correct** | **correct** |

The inversion flag is a polarity bit sent to the controller, not a "render normally" switch: which
setting looks right is a property of the panel. Diagnosed by drawing known colours and reading back
what appeared — exact complements (red as cyan, green as magenta, amber as dark blue) meant
inversion, not a wiring fault.

The orientation probe that settled the rotation half printed `tft.width()`/`tft.height()` to serial.
Reporting 320x240 while the glass showed portrait is what proved `setRotation` was not reaching the
panel, rather than the layout being wrong.

## Hardware notes from the first flash

- **This board's auto-reset does not drive IO0.** esptool can read the chip's boot log, so RX and
  the RTS reset line both work — but the board always boots `0x13` (normal SPI boot), meaning DTR
  never pulls IO0 low. Flashing therefore needs the manual sequence: unplug, hold `BOOT`, plug in,
  release — then flash with `--before no-reset`, or esptool's own reset knocks it straight back
  out of download mode. Worth putting in the setup README, because an adopter will hit it.
- **Partition scheme is `min_spiffs`** (1.9 MB app slots with OTA), chosen before the last USB
  flash because **OTA writes the app partition, not the partition table** — changing schemes later
  would have cost another manual flash. Current build uses 56% of a slot.
- **OTA needs an explicit host IP.** espota invites the device to connect *back* over TCP; with
  several interfaces present it otherwise advertises `0.0.0.0` and the upload dies with
  "No response from device". `tools/ota.sh` pins it.

## Why the plugin is split across two source trees

`plugin/src/core/` references no SimHub assembly; `plugin/src/simhub/` is two files that do.

This began as a constraint and turned out to be the right architecture. SimHub's assemblies are not
ours to redistribute, so they are gitignored, so **CI can never have them** — which would have made
the adapter conformance suite a local-only nicety rather than a gate. Putting every decision behind
`ITelemetrySource` means the suite, the fallback chain, the status ladder and the fault boundary all
build and run on a machine that has never seen SimHub.

It also makes check `R2` enforceable rather than aspirational: `tools/check_adapter_boundary.py`
sweeps the core for sim names and fails the build on a breach. Comments are exempt, because the
boundary is about what the code does.

## Mutation-testing the conformance suite

The suite passed 84 checks the first time it ran, which is when a test suite deserves the least
trust — this project has already produced one check that could not fail.

So nine deliberate defects were planted, one at a time, and the suite rebuilt against each: an
adapter claiming every title, the ordering guard removed, the positivity guard removed, unavailable
proximity collapsed to *none*, negative RPM passed through, the both-sides case broken,
out-of-domain enum values guessing *clear*, the fault latch removed, and the host's raw title name
leaked onto the wire.

**Eight were caught. One was not**, and it mattered: the case asserting that zeroed shift lights
fall through to the fraction rule was passing because of the *ordering* guard (`0 < 0` is false),
not the positivity guard it was supposed to exercise. The genuinely dangerous input is a first
light of `0` with a real shift point — strictly ordered, so only positivity stops it, and without
it the ramp start would be 0 rpm and the panel permanently lit. That case is now covered, and the
suite stands at 86 checks.

The hole existed because the operator asked what happens when the captured car is not the car being
driven. The question was about data; the answer was a missing test.

## Two defects found while building slice 6

Both were found by mechanical checks rather than by reading, which is the argument for having them.

**`titleId` had drifted to three spellings.** The owning concern's worked example says `iracing`;
the 23 shared fixtures and the replay tool said `iRacing`; SimHub's own game name is `IRacing`.
Only the first is the wire identity. The replay tool is supposed to be *indistinguishable from
live*, so a replayed frame carrying a different `titleId` than the plugin emits would have made
every replay-driven test subtly unlike the thing it substitutes for. Fixtures and the replay tool
are now aligned on `iracing`, and the conformance suite pins the two strings separately — the host
name and the wire identity — because conflating them is exactly how this happened.

**An `unsupportedTitle` frame was carrying a null `titleId`.** This was mine. I had the plugin core
withhold the running title's name on a leak-prevention rationale, which reads sensibly and
contradicts a stated contract: `INV-STATUS-CONSISTENT` exempts `titleId` from the
unavailable-when-not-live rule precisely so that this status can name the title, and
`ERR-UNSUPPORTED-TITLE` gives the device a screen to name it on. The firmware's `titleId[32]` field
carries a comment saying so. The device would have shown an empty screen.

The lesson recorded rather than the fix: a plausible-sounding rationale invented at the keyboard
does not outrank a contract, and the contract was two greps away.

## The dead-end the docs predicted, reached and closed

The doc set recorded an open risk: with no sim-PC address stored, the link ladder reports
`unresolved` forever and the only way out is to re-run provisioning. The panel then shipped into
exactly that state — `configuredHost: ""`, `linkState: "unresolved"` — and the first thing the new
`API-STATE` endpoint ever reported was the dead-end itself.

Slice 15 closes it. The configuration page is reachable at `http://cyd-sim-dash.local/` and changes
the address without re-provisioning. OTA does not depend on the stored address, which is what made
the recovery possible at all: the fix could be delivered to a device that could not be told
anything.

**The credential case had to be decided.** The panel's stored credential is empty, which
`SEC-CREDENTIAL-POLICY` forbids and the portal should have rejected — a recorded implementation gap.
The page is gated by that credential, so an empty one left three options: serve unauthenticated,
refuse and force re-provisioning, or serve and demand a credential before saving anything.

The third was taken. Refusing would have recreated the dead-end this slice exists to remove, and
serving permanently unauthenticated would perpetuate the violation. Serving once, and requiring the
violation to be repaired before any other change is accepted, is the only option that both rescues
the device and leaves it compliant.

## The configuration page carries all four fields

Resolved by operator decision 2026-09-23. The page now changes the WiFi network as well as the
sim-PC address and the credential.

The open question had been what to do when a network change fails, since the page is served *over
the network it is changing*: a successful change cannot deliver its own confirmation, and a failed
one leaves the device somewhere it cannot be reached. The obvious answer — verify and roll back — is
the wrong one, and the operator's reasoning is worth recording verbatim in substance: **a device can
never tell whether a connection failure is temporary or permanent, so it cannot safely roll back or
erase anything.** A rollback triggered by a router rebooting would discard a setting that was
correct.

So a failed join raises the captive portal and waits. That is not a new mechanism: it is exactly
what an unprovisioned device already does, and what a device does when its stored network is
unreachable at boot. The change adds a route into an existing recovery surface rather than a second
one.

Two consequences fell out of it. The credentials go to the platform's own WiFi store rather than
into `ENTITY-DEVICECONFIG`, so the passphrase exists in one place rather than two — `SEC-STORAGE-PLAIN`
already records it as recoverable with physical access, and a second copy would only widen that.
And `WiFi.begin()` cannot be called from the request handler, because it drops the association the
reply is travelling over; the change is staged and applied on the way to the restart.


## What the first end-to-end run on real hardware found

42 checks, all passing after four corrections. Three of the four were defects in the *test*, and
they are worth recording because each one looked exactly like a firmware bug until it was chased.

**Stamps derived from the wall clock went backwards.** Each test took its stamp base from
`time.time()`, but a test that advances synthetic stamps by +2000 in under two seconds of real time
leaves the next test's base *behind* the highest stamp the device has accepted. The device rejected
those frames as out-of-order — correctly, per `INV-STAMP-ORDER` — and three spotter checks failed
against firmware that was working perfectly. One monotonic allocator for the whole run removes the
class.

**A 5000 ms backwards jump is a producer restart, not an out-of-order frame.** `StampTracker`
accepts a jump larger than the staleness threshold on purpose: SimHub restarting resets its stamp
counter to near zero, and a naive newest-wins rule would then reject every subsequent frame forever,
leaving a dead panel with nothing on screen to explain it. The test has to ask for a *small*
backwards jump to exercise the rejection it is about.

**The HTTP endpoint stopped answering mid-probe, and it was not the firmware.** ICMP kept replying
while HTTP was dead, which looks like a hung loop — but the OTA push that followed succeeded, and
`ArduinoOTA.handle()` runs in that same loop, so the loop was alive throughout. The pattern is TCP
resource exhaustion from polling `/state` every 250 ms, and it cleared on its own. Worth knowing
before anyone builds a dashboard that polls this endpoint hard.

**The fourth was a contract that could not be satisfied.** See the `S8` amendment in Security:
nothing above 1472 bytes is ever delivered to the application on this hardware, so the 2 KB guard
cannot fire and its counter cannot increment. The guard stays as defence-in-depth; the check now
asserts what is observable.

## The first defect found by looking at it

The operator watched the flash and reported it was "drawn diagonally and quite annoying to look at,
instead of the entire frame changing at once". Nothing in 42 end-to-end checks could have found
this: every one of them reads `API-STATE`, and the state was correct the whole time. The pixels were
wrong, and only eyes can see that.

**Cause.** Repainting the background is ~123 KB over SPI, about 18 ms at 55 MHz, and the panel has
no tearing-effect signal wired for the firmware to synchronise against. The renderer then made it
worse in two ways: it filled the region and *then* drew the glyph on top, so two separate sweeps
crossed the display three times a second; and it repainted both edge bars on every background
change, although the background fill spans the gear region only and cannot alter an edge pixel.
That was two full-height fills per flash phase that could not change anything.

The diagonal appearance is the landscape rotation: the renderer's row-order writes run across the
panel's native scan direction, so the boundary between old and new content reads as a diagonal
rather than as a horizontal wipe.

**What changed.** The gear region is composed in an off-screen buffer and pushed as one transfer,
so nothing is ever half-drawn on the glass; the bars repaint only when they actually change; and
both paths are wrapped in single SPI transactions.

**What did not change: the bus time.** The same pixels still have to cross the wire, so the sweep
is still ~18 ms. What is fixed is that it is now *one* sweep instead of two-plus-two, with no
intermediate state visible. A genuinely atomic frame change is not achievable on this panel without
the TE signal, and claiming otherwise would be dishonest — if one sweep still reads as a wipe, the
remaining levers are the SPI clock and the flashed area, both of which have costs worth discussing
before spending them.

The buffer is 122 KB and WiFi has already taken its share of the heap, so the allocation is allowed
to fail and the direct-draw path remains. `API-STATE` reported `bufferedDraw` while that path existed — and it earned that place
immediately: the 16-bit attempt failed on the real device, and the field said so rather than the
panel simply misbehaving.

### The trap that followed, worth keeping written down

The obvious response to a failed 122 KB allocation is a cheaper colour depth, and the obvious
cheaper depth is 8-bit. **In TFT_eSPI an 8-bit sprite is RGB332, not a palette.** Only 4-bit sprites
are indexed. The API does not distinguish them: `createPalette()` is accepted on an 8-bit sprite and
quietly ignored, and drawing with indices `0` and `1` then produces RGB332 black and near-black.

The panel went entirely black, and nothing in the firmware could have reported it — the display
state was correct throughout, the endpoint answered normally, and `bufferedDraw` truthfully said
`true`. Only the glass was wrong.

The depth chain is now 16-bit, then 4-bit, and never 8-bit: besides the indexing question, RGB332
would have banded the blend that `U4` required at the time. `U4` now mandates discrete stops, so
that particular argument has expired even though the conclusion has not. Which mode is active is tracked in a flag
rather than inferred, because the two take different arguments and **neither complains about the
other** — an indexed sprite drawn with RGB565 values renders in whatever colours those numbers
happen to index, and a full-colour sprite drawn with indices renders black on black.

## The flash, settled by measurement after two wrong diagnoses

Worth keeping because the wrong turns were more instructive than the fix.

The operator reported the flash was "drawn diagonally and quite annoying". Three attempts:

1. **Composed the region off-screen** so the fill and glyph arrived as one transfer. Reasoned, not
   measured. No visible improvement — and the 16-bit buffer would not even allocate, while the
   4-bit fallback rendered the panel **entirely black**, because an 8-bit TFT_eSPI sprite is RGB332
   rather than indexed and the API does not distinguish them.
2. **Drew less often**, the operator's suggestion. Correct, and it fixed a real waste: the blend was
   recomputing on every RPM change, ~60 full repaints a second for colour steps nobody can see.
   Quantising to 48 steps cut that to 12.5/s. But the flash was unchanged, because drawing less
   often cannot shorten one sweep.
3. **Measured it.** 13.6 ms per draw against a ~15 ms panel refresh — write and scan at nearly the
   same rate, which is exactly the condition that makes a tear boundary crawl rather than flick past.
   The only lever left was area, and that was a product decision rather than an implementation one.

The operator chose bands top and bottom. Measured after: **4.3 ms**, about a quarter of a refresh.

Two things were discovered along the way and are worth keeping: the SPI clock was never the
configured 55 MHz — the ESP32 derives it as 80 MHz / N, so it was running at 40 — and a 4-bit
indexed buffer is *slower* than direct drawing, because pushing it costs a palette lookup per pixel
while `fillRect` streams one repeated colour and does no per-pixel work at all.

The lesson is the ordering: two reasoned fixes changed nothing the operator could see, and the
first measurement pointed straight at the answer. The panel now reports `lastDrawUs`, `worstDrawUs`
and `drawCount` through `API-STATE` so the next question of this kind starts with a number.

## The gear glyph: three faults in one line of code

The renderer chose its font as `(strlen(glyph) > 1) ? 7 : 8`. Found by the operator looking at the
panel on 2026-09-22, and every one of the three was invisible to the 42 end-to-end checks, because
all of those read `API-STATE` and the *state* was correct throughout. The glyph was right in the
frame, right in the display state, right over HTTP, and wrong on the glass.

**One: two typefaces.** Fonts 7 and 8 are different faces, so `9` and `10` did not look like the
same instrument. A display that changes style mid-shift reads as a fault rather than as data.

**Two: the `1` sat on the right.** Both are seven-segment faces, and a seven-segment `1` lights
only the two right-hand segments of its cell. That is correct for a real seven-segment display and
wrong here: in `18` it left a gap on the left and the pair looked shoved against the edge.

**Three, and the worst: `R` and `N` did not render at all.** TFT_eSPI's fonts 7 and 8 contain
`1234567890:-.` and nothing else. Two of the twenty values in the gear domain — and the two a driver
most needs to be certain of — drew nothing. There was no error, no warning, no failing check; the
library simply has no glyph and draws nothing.

All three are fixed by drawing everything in one proportional face, `FreeSansBold24pt7b`, sized
against the space the bands leave.

### The check already said so

`U3` reads: *"Every gear in the domain, **including `N`, `R`** and a two-character value, renders
within the gear region without clipping."*

The check was written correctly, named the exact failing cases, and had simply never been run — it
is a manual check, and nothing but a person looking at the panel can execute it. That is the
argument for the manual gate stated better than the doc set states it: the specification was right,
the code was wrong, and only the glass could tell the difference.

## Slice 23 is blocked on a five-second test

`CAP-BLANK` was authored doc-first on 2026-09-23 and is **not implementable until one thing is
known**: whether driving GPIO 21 low actually darkens this panel.

Some ESP32-2432S028R revisions hardwire the backlight on — the pin is present, defined as `TFT_BL`,
and does not gate the transistor. This build has only ever driven it HIGH, at `panel::begin`, so the
off state has never been observed on this hardware. Published pinouts say GPIO 21 is the backlight
control and is PWM-capable; they also say some revisions tie it on. Both can be true of different
boards.

The test is trivial and owed before any of the slice is built: blank the backlight for two seconds
at boot and look at the panel. If it stays lit, `CAP-BLANK` is not implementable on this hardware,
and the honest response is to retire the contract rather than ship a setting that does nothing. The
LDR on GPIO 34 would remain as a different feature, not a substitute.

The bindings for `CAP-BLANK` and `INV-BLANK-BOUND` are deliberately empty. An unbound new contract
is the planner's build-new signal, and pointing a locator at code that does not exist is how a
binding map starts lying.

## Two contracts with no natural home

`SEC-LAN-TRUSTED` and `SEC-NO-PERSONAL-DATA` are postures rather than behaviours, with no check and
no slice that would naturally realise them. They are **written determinations owed in slice 17**,
named here because they would otherwise be forgotten.

## Open decisions carried into the build

- ~~The panel's content while the provisioning access point is raised.~~ **Answered in slice 9a** —
  `panel::drawPortal` shows the access-point name, because `SCREEN-PORTAL` is HTTP and none of
  `SCREEN-LINK`'s nine conditions is true while unprovisioned, leaving the glass with nothing
  honest to show at the exact moment an adopter most needs telling what to do.
- Whether the manual pass (Definition of Done item 4) may be performed with the replay tool driving
  the panel, or requires live iRacing. Every glass property — colour, legibility, geometry, flash
  feel — is fully exercised by replayed frames. If it is read as requiring the sim, slices 11–15
  all acquire a rig dependency and the plan's parallelism collapses.
