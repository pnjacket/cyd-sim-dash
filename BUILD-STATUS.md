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
| `CAP-GEAR` | 11 | ☐ | — | `A2` · `U2` · `U3` · E2E replay |
| `CAP-SHIFT` | 12 | ☐ | — | `A3` · `U4` · `U5` · E2E replay |
| `CAP-SPOTTER-LEFT` | 13 | ☐ | — | `A4` · `U7` · `X2` · `X10` (live race) |
| `CAP-SPOTTER-RIGHT` | 13 | ☐ | — | `A4` · `U7` · `X2` · `X10` (live race) |
| `CAP-LINKSTATE` | 14 | ☐ | — | `A5` · `A6` · `U8` · `I13` |
| `CAP-PROVISION` | 9 | ☐ | — | `A9` · `I11` · `I12` · `U10` |
| `CAP-RECONFIG` | 15 | ☐ | — | `A7` · `U11` |
| `CAP-PUBLISH` | 19 | ☐ | — | `A8` · `I4` · `I7` · `I8` · `X1` |

## Slices

| # | Slice | Type | Built | Verified (stage) | Proof |
|---|---|---|:---:|---|---|
| 1 | Confirm the iRacing property mapping | verification-only | ◐ | **on-rig** (partial) | `X1` — captured 2026-09-22, 5 219 samples: every mapping row observed except the both-sides spotter case, which the session never produced. One correction found (`"IRacing"`, not `"iRacing"`) |
| 2 | Repository, CI and the records | cross-cutting | ✅ | off-device | pinned versions installed and matching the docs |
| 3 | Wire contract and shared fixtures | headless | ✅ | off-device | 30 fixtures, all behaving as their directory claims |
| 4 | Display-state engine | headless | ✅ | off-device | compiles clean; unit tier written, first run is CI |
| 5 | Plugin core, adapter, conformance suite | headless | ✅ | off-device | 86 conformance checks pass (C1–C7), 16 emitted frames validate against the wire schema, `R2` boundary check green. Suite mutation-tested: 9 deliberate defects, 9 caught — one hole found and closed |
| 6 | Publisher, device table, capture writer | headless | ✅ | off-device | 146 checks pass, including a real loopback socket round-trip (`I7`), registration idempotency and the 6 s expiry (`I8`), and `D9` searched against a real capture. Captures round-trip through the replay tool's own loader |
| 7 | Replay tool and synthetic fixtures | headless | ✅ | off-device | all 8 scenarios emit conformant frames; malformed set verified |
| 8 | Device bring-up: panel and boot screen | full | ✅ | **on-rig** | manual pass done 2026-09-21: border, corner colours and white-on-black all correct at 320x240. `U9` satisfied |
| 9 | Configuration store and provisioning portal | full | ◐ | off-device | portal raises, collects WiFi + host + credential, persists to NVS and survives reboot. Verification sequence and empty-field rejection still owed |
| 10 | Receive path, registration, handoff, `API-STATE` | full | ✅ | **on-rig** | 42/42 end-to-end checks pass against the panel, 2026-09-22: frames render, malformed input is counted, `I6` version mismatch and recovery, staleness, `ERR-UNKNOWN-PATH`. `S8` amended against measurement — see Security |
| 11 | Gear | full | ◐ | **on-rig, logically** | The glyph follows the frame, asserted through `API-STATE` on the panel. The *glass* checks `U2`/`U3` — legibility and clipping — still need eyes on the display |
| 12 | Shift ramp and flash | full | ◐ | **on-rig, logically** | All three phases confirmed on the panel: neutral below the ramp, ramping strictly inside 0..1 between the thresholds, flashing above. `U4`/`U5` are glass measurements and remain owed |
| 13 | Edge bars and composition | full | ◐ | **on-rig, logically** | Left-only, right-only and **both at once** all confirmed on the panel — the both-sides case a real track cannot stage. `U6` composition on the glass still owed |
| 14 | Link-state screen | full | ◐ | **on-rig, partly** | `unresolved`, `unreachable`, `stale`, `versionMismatch` and the null-while-driving case all observed on the panel. The nine composed icons are still owed |
| 15 | Configuration page, reconfiguration, erase | full | ◐ | **on-rig** (save path) | Used in anger: the page set the sim-PC address on a device that had none, closing the dead-end without re-provisioning. Erase, `S4`, `S6` and `S7` still owed |
| 16 | OTA and the update window | full | ◐ | off-device | OTA path **proven**: 0.1.1 delivered wirelessly and confirmed running. The credential gate and the update *window* are still owed |
| 17 | Security and error-catalogue verification | verification-only | ☐ | — | `I10` · `R3` · `Q8` · `S1`–`S12` |
| 18 | `E2E-STANDARD` conformance and journeys | verification-only | ☐ | — | `Q2` · `U10` |
| 19 | Live iRacing integration | full | ☐ | — | `A8` · `X1` · `X6` |
| 20 | Documentation, notice, provenance, release | cross-cutting | ☐ | — | `G1` · `G3` · `G5` · `G7` · `V5` · `V6` |
| 21 | Adopter unaided-setup trial | verification-only | ☐ | — | `A10` |
| 22 | Drive it and record the criteria | verification-only | ☐ | — | `A1` |

## Gate status

- **Definition-of-Done gate:** not yet evaluated — no slice has completed.
- **Build-ready gate:** **not passed.** `integrations-and-external-dependencies` is at `specified`
  against a `contract-grade` target, and all ten docs are `status: draft`. Slice 1 closes the
  first; the publish step closes the second.
- **Revoked Verified:** none. A bug adjudicated as a code defect revokes the affected slice's
  Verified here via a failing regression case, leaving the contract text untouched. An unreproduced
  bug writes nothing here.
