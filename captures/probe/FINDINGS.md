# Mapping capture — findings

**Captured** 2026-09-22 · iRacing · 5,219 samples over ~8.7 minutes · SimHub property inventory of
19,916 entries. Raw evidence in this folder; `samples.ndjson` is gitignored and regenerable.

This is the evidence for Integrations' check `X1`. Below, **observed** means it appears in the
capture; **derived** means it was computed from the capture and the fit is stated.

## What the capture settles

### iRacing publishes its own shift-light RPMs, and they are absolute — confirmed

| Property (under `GameRawData.SessionData.DriverInfo`) | Type | Observed |
|---|---|---|
| `DriverCarSLFirstRPM` | Double | 6130 |
| `DriverCarSLShiftRPM` | Double | 6690 |
| `DriverCarSLLastRPM` | Double | 6800 |
| `DriverCarSLBlinkRPM` | Double | 7210 |
| `DriverCarRedLine` | Double | 7500 |
| `DriverCarIdleRPM` | Double | 1000 |

All constant across the session, all revs per minute. The research call to prefer these over
SimHub's computed lights was correct.

`MaxRpm` read **7500**, exactly equal to `DriverCarRedLine`. The claim that SimHub's iRacing reader
sources `MaxRpm` from `DriverCarRedLine` is therefore confirmed against live data, not just
inferred.

### SimHub's computed shift lights carry no car-specific information — new, and stronger than assumed

`CarSettings_RPMShiftLight1` and `CarSettings_RPMShiftLight2` are 0.0–1.0 fractions, as research
said. What the capture adds is *what they are fractions of*, and the answer removes them as a
useful fallback:

| Property | Ramps over | As a fraction of `MaxRpm` | Fit error |
|---|---|---|---|
| `CarSettings_RPMShiftLight1` | 5625 → 6375 rpm | **0.750 → 0.850** | 7e-5 across 1,621 points |
| `CarSettings_RPMShiftLight2` | 6375 → 7125 rpm | **0.850 → 0.950** | 7e-5 across 1,822 points |

Those are round fractions of maximum RPM, and they know nothing about the car's actual shift
points — iRacing says shift at 6690, and light 1 finishes at 6375.

Round numbers to three decimals are not telemetry. These are SimHub-side settings, which SimHub
exposes per car in its own UI. That is the decisive argument against reading them, and it does not
depend on which car was captured: a value the operator configures cannot be the authority on what
the car does.

For this car they are also arithmetically identical to a fixed percentage-of-redline rule, which
the fallback chain already has as its final rung — so keeping them as a distinct input adds a step
that cannot produce a different answer.

### Two traps in properties whose names invite use

- **`Redline` reads 0** for iRacing, in every sample. The obvious-looking name is not populated.
  The redline is `DriverCarRedLine`, or equivalently `MaxRpm`.
- **`CarSettings_RedLineRPM` reads 7125**, which is exactly `0.95 x MaxRpm` — a SimHub-derived
  approximation, **not** iRacing's redline of 7500. Same for
  `CarSettings_CurrentGearRedLineRPM`, which read 7125 in every gear of the captured car. That is
  evidence about this car, not about the property — a car with real per-gear shift points may
  populate it.

### The game-identity string is `"IRacing"`, not `"iRacing"`

`DataCorePlugin.CurrentGame` returns the String **`IRacing`** — capital I, capital R. The doc set
had recorded `"iRacing"` from research. An adapter registry keyed on the documented spelling would
never match, and the failure would look like "no adapter for this title" rather than a typo.

### Gear is a string, with the domain already specified

Observed values: `1 2 3 4 5 6 N R`. Exactly the frame's specified gear domain; no conversion owed.
`CarSettings_MaxGears` read 6.

### Spotter: the computed fields and the raw enum agreed exactly

Every one of the 5,219 samples:

| `CarLeftRight` | `SpotterCarLeft` | `SpotterCarRight` | Samples |
|---|---|---|---|
| 0 (off) | 0 | 0 | 42 |
| 1 (clear) | 0 | 0 | 4,885 |
| 2 (car left) | 1 | 0 | 170 |
| 3 (car right) | 0 | 1 | 122 |

Two things follow. The raw `CarLeftRight` was **not** stuck at a constant — the reported SimHub
issue that motivated preferring the computed properties did not reproduce here. And over the
observed domain the two sources are interchangeable, so the primary/fallback choice is not urgent.

`SpotterCarLeftDistance`, `SpotterCarRightDistance`, `SpotterCarLeftAngle` and
`SpotterCarRightAngle` read **0 in every sample** — not populated for iRacing. They are not
available as a proximity refinement in v1, and v2's AC/ACC work should not assume them.

## What the capture does NOT settle

**The both-sides case.** `CarLeftRight` was observed only across `0..3`. Its documented domain
continues `4` = cars on each side, `5` = two cars left, `6` = two cars right. None occurred, because
the session never put the car between two others or two cars on one side.

This leaves one question genuinely open, and it is not a cosmetic one for this product: **when
`CarLeftRight` is 4, do `SpotterCarLeft` and `SpotterCarRight` both read 1?** If SimHub derives its
computed fields as `CarLeftRight == 2` and `== 3`, then the both-sides case would light *neither*
edge bar — the exact moment a driver most needs both.

The capture cannot distinguish the two derivations, because every observed sample is consistent with
both. Resolving it needs either another capture with a car on each side simultaneously, or a
decision to read `CarLeftRight` directly, where the mapping is unambiguous:

    left  = CarLeftRight in {2, 4, 5}
    right = CarLeftRight in {3, 4, 6}

`SpotterCarLeft` was observed only as `{0, 1}`, so the "does it count cars" question is likewise
**unresolved rather than answered** — two cars on one side never occurred. The existing
source-selection rule already makes this moot for v1 by mapping `> 0` to *one car*.

## The capture is of one car, and that is mostly fine

The operator raised this directly: the car driven was not the one they drive most.

It does not affect the mapping, because **no number from this capture is used as a value**. The
adapter reads `DriverCarSL*` live, per car, per session, and emits absolute RPM thresholds in every
frame; the firmware renders thresholds it is handed and never learns a car. The observed
6130/6690/7210/7500 are evidence that the properties *exist, are populated, and are in RPM* —
which is what `X1` asks for.

Two things it does limit, both recorded above rather than glossed:

- Whether **every** car populates `DriverCarSL*`. A car with no configured shift lights might
  report zeros. This is exactly what the fallback chain exists for, so it is a reason to keep the
  chain rather than a gap in the mapping — but the chain's first rung should check for a
  non-zero, ordered set of values rather than merely a present one.
- Whether `CarSettings_CurrentGearRedLineRPM` is ever per-gear. One car cannot say.

Capturing a second car — ideally the one actually driven — would close both cheaply, and needs
only the two-minute rev sweep, not the full script. It is not a v1 blocker.

## Still owed

**The SimHub version.** `SimHubWPF.exe` reports FileVersion `1.0.0.0`, the same placeholder its
assemblies carry, so reading the process did not work either. No property in the 19,916-entry
inventory carries it. It has to be read off SimHub's own UI.

## Note on the rig

The inventory shows a third-party plugin installed (`benofficial2.*`). It is unrelated to this
product and its properties are ignored, but it is worth knowing the capture came from a rig with
other plugins present rather than a clean install.
