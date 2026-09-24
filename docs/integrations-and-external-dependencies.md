---
artifact: product-doc
role: concern
concern-id: integrations-and-external-dependencies
behavior: module
trigger: third_party_deps
in-scope-subaspects: [per-external-contract, failure-modes-fallback-degradation, criticality, fidelity-substitution, version-pinning, data-mapping]
current-rung: specified
status: published
version: 0.7.0
---

# Integrations & External Dependencies — cyd-sim-dash

> SimHub, iRacing through it, three device libraries — and the property mapping the whole product rests on.

## Rung note

**This concern is at `specified`.** It went down after the first 2026-09-21 audit, was raised back
the same day, and the second audit reversed that raise with an argument I accept.

The version-pinning half of the raise stands: pinning is a decision the build makes, and the pins
below are real. What does not stand is the claim that the **data mapping** is finished. Two things
give it away. Check X1's pass condition is "its confidence replaced by a recorded value" — a
document edit rather than a product behaviour, which is unfinished authoring wearing a check's
clothes. And Delivery Process, itself at Contract-grade, schedules "write the iRacing data mapping
… every unconfirmed row in Integrations closes here" as **build step 1**, enforced by its check V9.
Two Contract-grade documents cannot disagree about whether a contract is finished, and Delivery's
version is operative because it is what a builder follows.

This is deliberate, not a defect to rush. The operator has no rig access and chose to find these
values while building rather than be blocked on them. The correct expression of that choice is a
doc that says `specified` and a playbook whose first step closes it — not a doc claiming
Contract-grade beside a playbook quietly finishing the authoring.

It reaches Contract-grade when build step 1 runs: every row observed once, the Confidence column
retired, and the spotter-source rule below confirmed.

## Purpose & Scope

This product is unusually dependency-shaped: it computes almost nothing itself. Its job is to take
values SimHub has already derived and put them on glass. That makes the **data mapping** — which
SimHub property feeds which frame field — the most load-bearing content here, and arguably in the
set. v1 needs exactly one such mapping: iRacing.

## Non-goals / Out-of-scope

Nothing is scoped out. Every published sub-aspect of this concern applies.

Two dependencies are **v2**, and no contract is owed for them now: Assetto Corsa / ACC, and Euro
Truck Simulator 2. [FUTURE-SCOPE] Each arrives as one adapter with its own mapping table. One ETS2
fact is already established and should not be re-investigated: the SCS telemetry SDK exposes only
the player's own truck, with no other-vehicle data at all, so its adapter will report proximity
unavailable permanently.

## Requirements

### Per-external contract

Eight dependencies, minted in *Contracts* with contract, auth, failure modes, fallback,
criticality, substitution and version pin. Four are build-time device libraries; three are the PC
chain; one supports development tooling only.

Authentication is **none, everywhere**. The plugin runs in-process inside SimHub, iRacing is reached
only through SimHub, and the libraries are compiled in. No dependency in this product involves a
credential, a token or an account.

### Failure modes + fallback/degradation

The governing rule, decided 2026-09-21: **a missing or renamed SimHub property degrades one
element, it does not fault the adapter.** If a shift-light property disappears in a SimHub update,
the ramp reports unavailable while gear and the bars keep working. This composes with the
already-specified frame semantics rather than adding a new mechanism — the element simply arrives
as `null`.

An adapter faults only when it throws, which the fault boundary catches. Per-element degradation is
the normal path; adapter fault is the exceptional one.

### Criticality

Stated per dependency in *Contracts*. Two are worth calling out. **SimHub is app-fatal** — without
it there is no product at all. **iRacing is also app-fatal in v1**, which is an unusual criticality
for a sim and is purely a consequence of it being the only supported title; in v2 each sim becomes
capability-degrading instead.

### Fidelity substitution

The replay tool substitutes for the **entire PC chain** — SimHub, the plugin, the adapter and the
sim — during firmware development and regression testing. Its fidelity is exactly the wire
contract: if it emits conformant frames, the firmware cannot distinguish them, which is the point.
It is also the forcing mechanism named by most rows of the error catalogue, because it can emit
frames that no real adapter would ever produce.

No substitution exists for the device libraries; they are compiled in and are either present or the
build fails.

### Version pinning

**Two build paths, two different strengths of pin** — stated together, because reading either alone
misleads. **CI** builds with `arduino-cli`, which *enforces* the versions below: a green run proves
the build used exactly them. **An adopter's Arduino IDE** enforces nothing, so for that path the
pins are *documentary* — the README records them and the adopter installs them by hand.

The versions are therefore enforced where the project controls the build and recorded where it does
not. That asymmetry is the accepted cost of the Arduino IDE toolchain choice, and it is why the
README's version table matters as much as the workflow's.

For SimHub itself the policy is the same shape: **record the tested version and claim no range.**
An adopter on a newer SimHub learns it does not work by it not working. This is the minimum honest
position, and it is worth stating plainly that it leaves the most likely breakage — a SimHub update
changing the SDK — undetectable until someone hits it.

**Pinning is a decision, not an observation.** The `arduino-cli` workflow *sets* the versions the
build uses, so the table below is chosen rather than discovered, and a green CI run is what proves
the documentation matches. The pins:

| Dependency | Pinned version | Note |
|---|---|---|
| ESP32 Arduino core | **3.3.12** | Released 2026-09-18, based on ESP-IDF 5.5 |
| TFT_eSPI | **2.5.43** | Released 2026-04-03 |
| ArduinoJson | **7.4.3** | Pinned exactly, like the others. 7.4.3 is *also* a floor no future bump may go below — a security constraint, stated separately so it survives version changes |
| WiFiManager (tzapu) | **2.0.17** | MIT, maintained by tablatronix |
| SimHub | **9.11.13** tested | Recorded, with no range claimed |
| Python (replay tool) | any 3.x | Development tooling only |

**The ArduinoJson pin is a security floor.** A buffer overrun in its string-to-float conversion,
reachable by a JSON document containing a string of many digits, affects **every version through
7.4.2** and is fixed in 7.4.3 (also back-ported to 7.3.2, 7.2.2, 6.21.6 and 5.13.6). The advisory
explicitly concerns programs exposed to untrusted input. This product's JSON parser is the **only**
place an untrusted host on the LAN reaches its code, so the floor is load-bearing: the 2 KB
datagram cap narrows the window but does not close it, since 2 KB of digits is more than enough.

### Data mapping

The mapping for iRacing, researched from public sources. Most rows are now **high confidence from documented sources**, but none has been observed against a
live session, so each carries its confidence and the exact observation that would confirm it. Per
the operator's standing rule, unconfirmed rows are deferred to first capture rather than guessed at.

Three findings from the research are worth stating before the table, because they change things:

**Gear is a string, not an integer.** SimHub exposes `Gear` with values `"R"`, `"N"`, `"1"`… — which
is exactly the domain already specified for the frame, so no conversion is owed and the
frame's gear field was specified correctly by luck as much as judgement.

**iRacing publishes its own shift-light RPMs, and they are absolute.** This is the finding that
matters most, and it replaces the earlier plan. Under
`DataCorePlugin.GameRawData.SessionData.DriverInfo`, iRacing supplies, per car, the exact RPMs its
own shift light runs on: `DriverCarSLFirstRPM` (first light), `DriverCarSLShiftRPM` (the sim telling
you to shift), `DriverCarSLLastRPM` (last light), `DriverCarSLBlinkRPM` (over-rev blink),
`DriverCarRedLine` and `DriverCarIdleRPM`. They are revs per minute, not fractions.

**Use those, not SimHub's computed shift lights.** `CarSettings_RPMShiftLight1` and
`CarSettings_RPMShiftLight2` are 0.0–1.0 progress values, and two independent sources describe them
as *SimHub's idea* of the car's lights rather than the car's own — computed from maximum-RPM
settings rather than supplied by the game. Driving the shift cue from them would mean tuning
against a heuristic; driving it from `DriverCarSL*` means using what the car itself says.

**Capture 2026-09-22 confirms it and identifies what they actually are.** Fitted against 5,219
live samples, light 1 ramps linearly over 0.750–0.850 of `MaxRpm` and light 2 over 0.850–0.950, to
within 7e-5 — round fractions of maximum RPM, and unrelated to the car's real shift point, which
iRacing put at 6690 rpm while light 1 finished at 6375.

Round numbers to three decimal places are not telemetry. They are **SimHub-side settings**, which
SimHub exposes per car in its own UI and an operator can edit. That is the decisive argument, and
unlike a claim about their value it does not depend on which car was driven: a property the
*operator* configures cannot be the authority on what the *car* does. Reading it would make the
shift cue depend on a SimHub setting the adopter may never have seen.

The adapter therefore **does not read them at all**, and the fallback chain drops straight from the
native values to its own percentage rule — which is what these encode anyway, for this car.

[ASSUMPTION] That the fractions are settings rather than a per-car computation is inferred from
their round values and from SimHub exposing equivalent settings in its UI; it was observed in one
car. It changes nothing, because the conclusion is *not to read them* either way.

**The redline gap is closed, and confirmed against live data.** SimHub's iRacing reader takes
`DriverCarRedLine` as `MaxRpm`; the capture read both as 7500 in every sample. No amendment to
Domain & Data's chain is needed.

**Two properties invite use and must not be used.** `Redline` — the obvious name — read **0** in
every sample; it is simply not populated for iRacing. And `CarSettings_RedLineRPM` read 7125, which
is exactly `0.95 x MaxRpm`: a SimHub approximation, not iRacing's redline of 7500.
`CarSettings_CurrentGearRedLineRPM` read the same 7125 in all six gears **in the captured car**.
That does not mean the property is flat in general — a car with genuinely different per-gear shift
points may well populate it, and one sample cannot distinguish "the property is useless" from "this
car has one shift point". The per-gear [FUTURE-SCOPE] item below therefore stays open, and this row
is evidence about one car rather than about the property.

**A complete derivation is available for proximity**, and it is specified below rather than
deferred: iRacing's own `CarLeftRight` enum maps exactly onto the per-side domain already owned by
Domain & Data.

## Open Questions

- ~~The redline value source.~~ **Resolved by research 2026-09-21** — `MaxRpm` is sourced from
  iRacing's `DriverCarRedLine`, so the fallback chain's redline has a concrete source and needs no
  amendment.
- ~~What the shift-light fractions are a fraction of.~~ **Moot** — the adapter uses iRacing's native
  absolute RPMs and never multiplies a fraction.
- ~~Whether `SpotterCarLeft` / `SpotterCarRight` **count** cars on a side.~~ **Moot since 2026-09-22.** The adapter reads the raw `CarLeftRight` enum and does not consult the computed fields at all, so their upper domain no longer affects anything this product does. Left recorded rather than deleted because the question would otherwise be asked again by the next person to read the mapping table.

  **Source-selection rule — superseded 2026-09-22.** This previously had the adapter read the
  computed properties, falling back to the raw enum. That is now reversed: the enum is the primary
  source and the computed properties are unused. See the resolved entry below for the derivation
  table and the reasoning. The *never emits two cars in v1* part of the rule survives the reversal
  unchanged.
- ~~The exact string `DataCorePlugin.CurrentGame` returns for iRacing.~~ **Resolved, and corrected by
  capture 2026-09-22** — the value is `"IRacing"`, capital I and capital R. Research had recorded
  `"iRacing"`. A registry keyed on the documented spelling would never match, and would report
  "no adapter for this title" rather than a typo, so the exact casing is load-bearing.
- ~~Every version pin.~~ **Resolved** — pinned by decision above; the workflow enforces them.
- ~~The both-sides spotter case.~~ **Resolved by decision 2026-09-22: the iRacing adapter reads
  `GameRawData.Telemetry.CarLeftRight` directly and derives both sides from the enum.** This
  reverses the earlier source-selection rule, which preferred SimHub's computed
  `SpotterCarLeft` / `SpotterCarRight`.

  The derivation is total over the enum's documented domain, so no value can fall through:

  | `CarLeftRight` | Meaning | `spotterLeft` | `spotterRight` |
  |---|---|---|---|
  | 0 | spotter off | `none` | `none` |
  | 1 | clear | `none` | `none` |
  | 2 | car left | `one` | `none` |
  | 3 | car right | `none` | `one` |
  | 4 | cars **both sides** | `one` | `one` |
  | 5 | two cars left | `one` | `none` |
  | 6 | two cars right | `none` | `one` |

  Values 5 and 6 map to `one` rather than `two` because the bars are on-off and v1 never renders a
  count — the existing rule, unchanged. Any value outside `0..6` is treated as `none` and counted as
  a soft fault, so an unexpected value degrades rather than faults.

  **Why this way round.** Rows `4..6` were never observed and, per the operator, are impractical to
  stage on purpose. So the choice is between a derivation that is *defined* for the both-sides case
  and one that is *unverifiable* for it — because if SimHub computes its fields as
  `CarLeftRight == 2` and `== 3`, then with a car on each side **neither edge bar would light**, a
  silent failure of two capabilities at the moment they matter most. The reliability concern that
  originally motivated preferring the computed fields did not reproduce: the raw enum tracked
  correctly through all 5,219 captured samples.

  It also fits the architecture rather than straining it. The title-adapter pattern exists so that
  title-specific sources stay behind the adapter; the firmware receives the same normalised frame
  either way, and a future AC/ACC adapter remains free to use SimHub's computed fields.

  **[ASSUMPTION] The enum semantics for `4..6` are taken from iRacing's published SDK, not from
  this capture.** They are the only rows not observed.

  **What would falsify this**, stated so the observation is diagnostic rather than vague: driving
  side by side with one car **fails to light the corresponding bar**, or being between two cars
  lights only one bar or neither. Either symptom means the enum is not behaving as documented on
  this rig, and the fallback is to read the computed fields for rows `2..3` while keeping the enum
  for `4..6`. Recorded as a decision under uncertainty with a named observable, not as a
  confirmed fact.
- [GAP] SimHub's behaviour toward a plugin that throws during load, as opposed to during an update.
- [FUTURE-SCOPE] SimHub exposes per-gear upshift RPM values through a per-gear redline setting.
  That would allow per-gear shift thresholds rather than one pair per car, which is a real
  improvement in shift feel and needs no wire change — the adapter would simply resolve different
  absolute values per gear.

### The `CarLeftRight` enum, corroborated by research 2026-09-23

The adapter reads iRacing's raw enum rather than SimHub's computed spotter fields, and rows `4..6`
of that derivation rested on the published SDK rather than on anything observed. A search for prior
art was cheaper than waiting for a race, and it moved three things.

**The enum's definition is confirmed by three independent sources**, including iRacing's own
`irsdk_defines.h` and two long-standing third-party readers (CrewChief, SIMRacingApps):

    0 LROff · 1 LRClear · 2 LRCarLeft · 3 LRCarRight
    4 LRCarLeftRight "there are cars on each side"
    5 LR2CarsLeft · 6 LR2CarsRight

**The derivation this product uses is the one the community uses.** Independent write-ups describe
detecting a car on the left by testing the value against `2, 4, 5` — which is exactly
`left = {2, 4, 5}` as specified above, arrived at separately. That is corroboration of the *mapping*,
which is the part that was assumed.

**The stuck-at-a-constant concern is real, reported, and did not reproduce here.** SimHub issue
#1395 (August 2023) reports `CarLeftRight` pinned at `1` while iRacing's voice spotter worked
correctly; it was closed as `gameLimitation` rather than a SimHub defect. That report is what
originally argued for preferring SimHub's computed fields. Against it: the capture of 2026-09-22
tracked the enum correctly across 5,219 samples, and check `A4` passed on the rig on 2026-09-23 with
the live spotter. So the failure exists somewhere, but not in this setup — and it is worth knowing
that if the bars ever go permanently dark while the in-sim voice spotter still calls, this is the
first thing to suspect and it is not a fault in this product.

**What research could not settle: whether value `4` is ever actually emitted.** No source reports
observing it; every source only documents that it is defined. `X10` therefore stands, but it is now
a narrower question — not "is this derivation right" but "does iRacing raise the both-sides value in
a real race".

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Domain & Data | the frame and spotter-state entities the mapping targets, and the fallback chain the adapter implements |
| Architecture | the adapter and publisher components; the fault boundary that catches an adapter throwing; the replay component that substitutes for this whole chain |
| Interfaces & Contracts | the wire contract that defines what a faithful substitution means |
| Product & Requirements | the capabilities each dependency serves, and the adopter persona the version documentation exists for |
| Quality & Testing | the adapter conformance suite each adapter must pass |

## Examples / Worked scenarios

**Resolving a live frame.** The adapter reads `Gear` as `"4"` and `Rpms` as 7200, then takes
`DriverCarSLFirstRPM` (7800) and `DriverCarSLShiftRPM` (8400) **directly as absolute RPM** — no
multiplication, because iRacing publishes these in revs per minute. It reads the spotter state and
returns a frame with status live. Nothing sim-specific crosses the wire.

**A SimHub update renames the shift-light properties.** The adapter's lookups return nothing. It
reports ramp-start and flash as unavailable and continues. The panel shows gear and both bars
normally, with no shift ramp. The operator sees a working panel missing one element rather than a
dead one — and the soft-fault counters on the configuration page do not move, because nothing
malformed was sent.

**A SimHub update breaks the SDK surface entirely.** The adapter throws. The fault boundary catches
it, logs once, and the publisher sends frames with status `adapterFault`. The panel names the
fault. SimHub itself is unaffected.

**Developing with everything shut.** The replay tool sends a captured lap. The firmware behaves
identically to a live session, because the substitution is at the wire contract. The same tool then
sends a frame with inverted thresholds to prove the device rejects it.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| A missing property degrades one element, never faults the adapter | Losing the ramp is better than losing the panel; it reuses the unavailable semantics already in the frame | A silently missing element looks like a title limitation rather than a regression |
| Record tested versions; no vendoring, no second build manifest | Minimum that the setup documentation already owes | Reproducibility depends entirely on an adopter reading carefully and installing by hand |
| State SimHub's tested version, claim no range | Honest and free | The most likely breakage — a SimHub update — is undetectable until someone hits it |
| ~~Prefer the computed spotter properties~~ → **read the raw `CarLeftRight` enum**, reversed 2026-09-22 on capture evidence | The enum's derivation is **total**, so the both-sides case is defined rather than assumed; the reported stuck-constant issue did not reproduce across 5 219 samples | Rows `4..6` of the enum rest on iRacing's published SDK rather than observation, and are impractical to stage deliberately. Carried as a decision under uncertainty with a named falsifying observation, not as a confirmed fact |

## Contracts

### External dependencies

| ID | Dependency | Contract · auth | Failure mode → fallback | Criticality | Substitution | Version |
|---|---|---|---|---|---|---|
| `DEP-SIMHUB` | SimHub application | Hosts the plugin in-process; supplies all telemetry · no auth | Not running → no frames; device shows link state | **app-fatal** | Replay tool, at the wire contract | 9.11.13 tested, no range claimed |
| `DEP-SIMHUB-SDK` | SimHub plugin assemblies | Three DLLs referenced from the SimHub install — `GameReaderCommon.dll`, `SimHub.Logging.dll`, `SimHub.Plugins.dll` · no auth | Surface changes → adapter throws → caught, status `adapterFault` | **app-fatal** | none | ships with SimHub 9.11.13 |
| `DEP-IRACING` | iRacing, via SimHub | Supplies gear, RPM, the `DriverCarSL*` shift points and spotter state · no auth | Not running → status `noSim` | **app-fatal in v1** — the only supported title; capability-degrading from v2 | Replay tool | **Unpinnable, and a real exposure**: iRacing updates per season and the whole mapping is against its surface. Nothing here holds it still. Mitigations: a changed property degrades one element rather than faulting the adapter, and captures preserve a known-good frame stream to compare against |
| `DEP-ESP32-CORE` | ESP32 Arduino core | Device platform, WiFi stack, NVS, OTA · no auth | Absent → build fails | app-fatal | none | **3.3.12** |
| `DEP-TFT-ESPI` | TFT_eSPI | Panel drawing · no auth | Misconfigured pins → blank screen, **no error** | app-fatal | none | **2.5.43** |
| `DEP-ARDUINOJSON` | ArduinoJson | Parses untrusted datagrams · no auth | Parse failure → soft fault, counted | app-fatal | none | **≥ 7.4.3 — security floor** |
| `DEP-WIFIMANAGER` | WiFiManager (tzapu) | Captive portal · no auth | Absent → build fails | app-fatal | none | **2.0.17** |
| `DEP-PYTHON` | Python runtime | Runs the replay tool · no auth | Absent → no replay tooling; the product is unaffected | **fail-soft** — development tooling only | none | any 3.x. **Deliberate override**: the standard carves build/test-time tooling out of `DEP-###` and into Operations' tooling register, and Operations is out of scope here — so it is recorded as a dependency rather than dropped, which would have been the only alternative |

`DEP-TFT-ESPI` carries the single most likely cause of an adopter failing to get a working panel:
its pin configuration lives in the library's own header rather than in the sketch, and getting it
wrong produces a blank screen with no error message of any kind. The setup README must treat this
as the primary failure mode, not a footnote.

### iRacing data mapping

**Observed against a live session on 2026-09-22** — 5,219 samples over ~8.7 minutes, plus a
19,916-entry property inventory. Evidence and full analysis in
[`captures/probe/FINDINGS.md`](../captures/probe/FINDINGS.md).

The raw frame capture and the property inventories are **not in the repository**. The inventories
dump every property with its live value, which includes the name and customer ID of every driver on
track — other people, who did not agree to appear here. `FINDINGS.md` and `summary.txt` carry the
distilled record instead, and nothing in the mapping table depends on the raw files. Re-running the
probe regenerates them.

Every row below is confirmed except the one marked otherwise, which is the both-sides spotter case:
it needs a car on each side simultaneously, and the session never produced one.

| Frame field | Source property | Type | Status | Observed |
|---|---|---|---|---|
| `gear` | `NewData.Gear` | **string** — `"R"`, `"N"`, `"1"`… | **confirmed** | domain `1 2 3 4 5 6 N R` — exactly the specified frame domain, no conversion owed |
| `rpm` | `NewData.Rpms` | double, RPM | **confirmed** | 0 – 7483 across the session |
| `rampStartRpm` | `GameRawData.SessionData.DriverInfo.DriverCarSLFirstRPM` | double, **absolute RPM** | **confirmed** | 6130, constant |
| `flashRpm` | `GameRawData.SessionData.DriverInfo.DriverCarSLShiftRPM` | double, **absolute RPM** | **confirmed** | 6690, constant |
| *(fallback input)* redline | `NewData.MaxRpm`, which SimHub's iRacing reader sources from `DriverCarRedLine` | double, RPM | **confirmed** | both read 7500, in every sample — the two are the same quantity |
| ~~*(fallback input)* computed lights~~ | ~~`CarSettings_RPMShiftLight1` / `2`~~ | fraction 0.0–1.0 | **removed** | fixed ramps over 0.750–0.850 and 0.850–0.950 of `MaxRpm`, identical for every car. Arithmetically the same as the chain's own percentage rule, so it is no longer a distinct rung |
| `spotterLeft` / `spotterRight` | **`GameRawData.Telemetry.CarLeftRight`** | int enum `0..6` | **primary source, by decision 2026-09-22** | observed `0 1 2 3`, transitioning correctly with a car alongside — not stuck at a constant. Rows `4..6` unobserved; derivation is total by the table in *Open Questions* |
| *(no longer used)* | ~~`GameData.SpotterCarLeft` / `SpotterCarRight`~~ | integer, `> 0` means present | **superseded** | `{0, 1}` only, agreeing with the enum in all 5,219 samples — but their handling of the both-sides case cannot be verified, which is why they were dropped |
| — | `SpotterCarLeft/RightDistance`, `…Angle` | double | **unavailable** | **0 in every sample** — not populated for iRacing. Not a proximity refinement in v1, and v2's AC/ACC work must not assume them |
| `status` = `noSim` | `data.GameRunning` | bool | **confirmed** | false before the session, true on track |
| `titleId` | `DataCorePlugin.CurrentGame` | string game id, **`"IRacing"`** | **confirmed, and corrected** | literally `IRacing` — capital I, capital R. Research had recorded `"iRacing"`; a registry keyed on that spelling would never match |

**Three more native values are available and deliberately unused in v1**, recorded so they are not
rediscovered later: `DriverCarSLLastRPM` (last light), `DriverCarSLBlinkRPM` (the RPM at which
iRacing's own lights blink — a ready-made over-rev threshold, and the shift element has no over-rev
state by decision) and `DriverCarIdleRPM`.

**The ordering is ascending: First → Shift → Last → Blink** — a worked example from the iRacing
SDK reads 7800 / 8400 / 8800 / 9000. iRacing's own lights fill between *First* and *Last*, change to
the shift colour at *Shift*, and blink above *Blink*.

That settles the mapping and its rationale. This product ramps **First → Shift** and flashes at
*Shift*, because *Shift* is the sim telling you to shift and that is precisely what the flash means
here. Ramping to *Last* would delay the flash past the recommended shift point, and flashing at
*Blink* would tell you after you should already have changed gear.

**Proximity derivation from the raw enum**, should the computed properties prove unusable. This is
complete and needs no further research — iRacing's enum maps exactly onto the owned per-side
domain:

| Raw value | Meaning | `spotterLeft` | `spotterRight` |
|---|---|---|---|
| 0 | off / unavailable | `null` | `null` |
| 1 | clear | `none` | `none` |
| 2 | car left | `one` | `none` |
| 3 | car right | `none` | `one` |
| 4 | cars both sides | `one` | `one` |
| 5 | two cars left | `two` | `none` |
| 6 | two cars right | `none` | `two` |

Value 0 mapping to `null` rather than to `none` is the important row: iRacing distinguishes *no
information* from *clear*, and so does this product.

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| X1 | Every row of the mapping table is observed once against a live iRacing session, and its confidence replaced by a recorded value | the mapping is real rather than researched |
| X2 | **All seven enum rows** `0..6` map to the specified `spotterLeft` / `spotterRight` pair, plus an out-of-domain value mapping to `none` and counting a soft fault. Asserted as a **table-driven unit test feeding `CarLeftRight` directly** — no rig, no sim, no staged traffic. Rows `2` and `3` are additionally corroborated by the capture of 2026-09-22; rows `4..6` rest on the SDK's documented semantics, which is what makes the live observation in `X10` worth doing | proximity derivation |
| X3 | With no car alongside, the emitted spotter fields are `none`, not `null`; with the raw enum reading 0, they are `null` | the clear-versus-unavailable distinction |
| X4 | Removing or renaming a mapped property in a test harness results in that element alone arriving as `null`, with every other element unaffected | per-element degradation |
| X5 | An adapter forced to throw produces status `adapterFault`, one log entry rather than a stream, and leaves SimHub running | `DEP-SIMHUB-SDK` failure mode |
| X6 | Stopping the sim while SimHub runs produces status `noSim` | `DEP-IRACING` failure mode |
| X7 | The replay tool drives a device through a full captured lap with SimHub, the plugin and the sim all shut, and the device's behaviour is indistinguishable from live | fidelity substitution |
| X8 | The README lists an exact version for the ESP32 core and each of the three device libraries, and a clean machine following it produces a working panel | version pinning by documentation |
| X9 | The recorded SimHub tested version matches the one the plugin was built against | `DEP-SIMHUB` version policy |
| X10 | **Accepted by the operator 2026-09-23 without the live observation.** The check asked that a real race confirm both bars light when between two cars; the operator judged the wait disproportionate — the situation is rare, easy to miss while actually racing, and would have held the doc set open indefinitely.<br><br>**What the acceptance rests on, and what it does not.** The enum's definition is corroborated by iRacing's own `irsdk_defines.h` and two independent third-party readers, and the `left = {2,4,5}` derivation matches what other projects arrived at separately — so the *specification* is well evidenced. What remains unobserved is whether iRacing **emits** value `4` in practice. No source reports seeing it; every source only documents that it is defined.<br><br>This is therefore **accepted, not verified**, and the distinction is load-bearing: if iRacing never raises `4`, the both-sides case silently lights neither bar. The symptom would be both bars dark while the in-sim voice spotter calls a car on each side. If that is ever seen, this row is the first place to look | the one assumption in the spotter derivation |

