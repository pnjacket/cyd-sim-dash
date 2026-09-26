# Build-status record

The **Verified-rung / implementation-status** record owned by Delivery Process. Deliberately
**separate from `docs/manifest.yaml`**: the manifest tracks how complete the *docs* are, this file
tracks what is *built and proven*. Doc maturity is not implementation status, and conflating them
is how a doc set starts claiming a product that does not exist.

This file therefore contains **no documentation rung**, and the manifest contains no build state.
Check `V4` asserts exactly that, field by field.

History rides on git; this holds *current* status only.

**Stage key.** There is one Definition-of-Done gate, not two — the developer stage and the real
stage are the same rig, and there is no environment to promote through. `off-device` means the CI
tiers passed; `on-rig` means the manual pass was also done, which is required for anything touching
the glass.

## Capability coverage

Every capability in the gate scope must appear here as a built-and-verified slice. None may be
built in a single layer.

| `CAP-###` | Completing slice | Built | Verified (stage) | Proof |
|---|---|:---:|---|---|
| `CAP-GEAR` | 11 | ✅ | **on-rig** | `U2` and `U3` passed 2026-09-23 · E2E replay green · `A2` still wants the live sim |
| `CAP-SHIFT` | 12 | ✅ | **on-rig** | `U4` passed 2026-09-23 · `U5` accepted by judgement, tolerance unverified · `A3` still wants the live sim |
| `CAP-SPOTTER-LEFT` | 13 | ✅ | **on-rig** | `U7` passed 2026-09-22 · `A4` passed on-rig 2026-09-23 · `X2` all seven enum rows · **`X10` accepted without observation** 2026-09-23 — see Integrations for what that rests on |
| `CAP-SPOTTER-RIGHT` | 13 | ✅ | **on-rig** | `U7` passed 2026-09-22 · `A4` passed on-rig 2026-09-23 · `X2` all seven enum rows · **`X10` accepted without observation** 2026-09-23 — see Integrations for what that rests on |
| `CAP-LINKSTATE` | 14 | ☐ | — | `A5` · `A6` · `U8` · `I13` |
| `CAP-PROVISION` | 9 | ☐ | — | `A9` · `I11` · `I12` · `U10` |
| `CAP-RECONFIG` | 15 | ☐ | — | `A7` · `U11` |
| `CAP-PUBLISH` | 19 | ✅ | **on-rig** | `A8` passed 2026-09-23 against live iRacing · `I7`/`I8` on the loopback suite · `X1` closed by the capture |

## Slices

| # | Slice | Type | Built | Verified (stage) | Proof |
|---|---|---|:---:|---|---|
| 1 | Confirm the iRacing property mapping | verification-only | ✅ | **on-rig** | `X1` closed by the capture of 2026-09-22 — 5 219 samples, every mapping row recorded. Three research answers were corrected against measurement, including `"IRacing"` rather than `"iRacing"` |
| 2 | Repository, CI and the records | cross-cutting | ✅ | off-device | pinned versions installed and matching the docs |
| 3 | Wire contract and shared fixtures | headless | ✅ | off-device | 30 fixtures, all behaving as their directory claims |
| 4 | Display-state engine | headless | ✅ | off-device | compiles clean; unit tier written, first run is CI |
| 5 | Plugin core, adapter, conformance suite | headless | ✅ | off-device | 86 conformance checks pass (C1–C7), 16 emitted frames validate against the wire schema, `R2` boundary check green. Suite mutation-tested: 9 deliberate defects, 9 caught — one hole found and closed |
| 6 | Publisher, device table, capture writer | headless | ✅ | off-device | 146 checks pass, including a real loopback socket round-trip (`I7`), registration idempotency and the 6 s expiry (`I8`), and `D9` searched against a real capture. Captures round-trip through the replay tool's own loader |
| 7 | Replay tool and synthetic fixtures | headless | ✅ | off-device | all 8 scenarios emit conformant frames; malformed set verified |
| 8 | Device bring-up: panel and boot screen | full | ✅ | **on-rig** | manual pass done 2026-09-21: border, corner colours and white-on-black all correct at 320x240. `U9` satisfied |
| 9 | Configuration store and provisioning portal | full | ◐ | off-device | portal raises, collects WiFi + host + credential, persists to NVS and survives reboot. Verification sequence and empty-field rejection still owed |
| 10 | Receive path, registration, handoff, `API-STATE` | full | ✅ | **on-rig** | 42/42 end-to-end checks pass against the panel, 2026-09-22: frames render, malformed input is counted, `I6` version mismatch and recovery, staleness, `ERR-UNKNOWN-PATH`. `S8` amended against measurement — see Security |
| 11 | Gear | full | ✅ | **on-rig** | **`U3` passed by operator observation 2026-09-23**: every gear in the domain renders without clipping, including `N`, `R` and the two-character cases. Drawn in one proportional face and sized once against the widest value, so every gear is the same size. `U2` is satisfied by construction — the glyph sits on permanent black |
| 12 | Shift ramp and flash | full | ✅ | **on-rig** | All three phases confirmed. Reworked twice against operator feedback and measurement: bands rather than full-field, four discrete stops rather than a blend, 80 MHz SPI. Draw time **13.6 ms → 4.3 ms**, redraws **~60/s → 3.6/s**. Operator accepts the residual tearing and **waived `U5`'s slow-motion measurement** 2026-09-23, judging the rate acceptable by eye — so the ± 10 % tolerance is unverified by choice. `U4` passed with the discrete stops |
| 13 | Edge bars and composition | full | ✅ | **on-rig** | **`U6` and `U7` passed by operator observation 2026-09-22**, driven by the `composition` and `spotter` scenarios: a lit bar stays solid white through both flash phases, and each side lights the correct bar. Both-sides-at-once was rendered and confirmed here — the case a real track cannot stage on demand. The bands are confined to the gear region, so `U6` now holds by geometry as well as by draw order |
| 14 | Link-state screen | full | ◐ | **on-rig, partly** | `unresolved`, `unreachable`, `stale`, `versionMismatch` and the null-while-driving case all observed on the panel. The nine composed icons are still owed |
| 15 | Configuration page, reconfiguration, erase | full | ◐ | **on-rig** (save path) | Used in anger: the page set the sim-PC address on a device that had none, closing the dead-end without re-provisioning. Erase, `S4`, `S6` and `S7` still owed |
| 16 | OTA and the update window | full | ◐ | off-device | OTA path **proven**: 0.1.1 delivered wirelessly and confirmed running. The credential gate and the update *window* are still owed |
| 17 | Security and error-catalogue verification | verification-only | ☐ | — | `I10` · `R3` · `Q8` · `S1`–`S12` |
| 18 | `E2E-STANDARD` conformance and journeys | verification-only | ☐ | — | `Q2` · `U10` |
| 19 | Live iRacing integration | full | ◐ | **on-rig** | **First live run 2026-09-23.** `A2`, `A3`, `A4` and `A8` all pass against real iRacing telemetry — gear mapping, the ramp through to the flash, per-side proximity, and values agreeing with SimHub's own display. `X10` outstanding: it needs two cars alongside at once, which a practice session did not produce |
| 20 | Documentation, notice, provenance, release | cross-cutting | ☐ | — | `G1` · `G3` · `G5` · `G7` · `V5` · `V6` |
| 21 | Adopter unaided-setup trial | verification-only | ☐ | — | `A10` |
| 22 | Drive it and record the criteria | verification-only | ☐ | — | `A1` |
| 23 | Backlight blanking | full | ✅ | **on-rig** | `Q12`–`Q15`. GPIO 21 darkens this panel — confirmed 2026-09-26, which unblocked this slice and slice 24. Blanked at ~48 s with the period at one minute and woke on telemetry. Schema 2 migrated in place: host and credential survived, `blankAfterMinutes` arrived at its default. First real exercise of `INV-CONFIG-MIGRATION` |
| 24 | Wake the rig from the panel | full | ◐ | **on-bench** | **`Q16` passed on the wire 2026-09-26** — two touches from a blanked `unreachable` panel produced **exactly one** 102-octet datagram from the panel's address on UDP 9: six `0xFF`, then the learned address sixteen times, identically. The first touch put nothing on the wire, which is the half the glass cannot show. `Q19` passed in part: the address was learned from the ARP cache on the first accepted frame and survived a reboot — but a firmware restart, not the rig power-cycle the check asks for. `Q17` and `Q18` hold at the unit tier against the pure sequence; neither has been provoked on the device. **`Q20` is the open one** and needs the rig — the only check here that can fail for reasons outside this product |

## Gate status

- **Definition-of-Done gate:** not yet evaluated — no slice has completed.
- **Build-ready gate:** **passed 2026-09-23.** All ten in-scope concerns are at `contract-grade`
  against a `contract-grade` target, and all ten are `status: published`. The last movement was
  Integrations, held at `specified` until `X1` — every row of the mapping table observed against a
  live session — was closed by the capture of 2026-09-22.

  Publishing is a claim about the *documents*, not the product. Two checks were closed by operator
  judgement rather than by their specified method, and both say so where they live: `U5`'s
  slow-motion flash measurement was waived, and `X10` was accepted without the live race it asks
  for. Neither is recorded as verified.
- **Revoked Verified:** none. A bug adjudicated as a code defect revokes the affected slice's
  Verified here via a failing regression case, leaving the contract text untouched. An unreproduced
  bug writes nothing here.
