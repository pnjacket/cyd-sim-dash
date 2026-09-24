# cyd-sim-dash — Documentation Set

> **Derived index.** Generated from [`manifest.yaml`](manifest.yaml), which is the source of
> truth. If the two disagree, the manifest wins and this file is stale — the `doc-maturity-auditor`
> flags the drift. Regenerate this index whenever the manifest changes.

A four-element sim-racing dashboard on an ESP32 Cheap Yellow Display, fed over WiFi by a
purpose-built SimHub plugin. Centre gear indicator, background shift ramp and flash, and left and
right edge bars for cars alongside — all four rendering at once.

- **Authored against:** Dictum `v1.2.0`
- **Packaging:** per-concern
- **Scaffolded:** 2026-09-21 · **Last audited:** 2026-09-21

## Concerns in scope

| Concern | Behavior | Rung | Target | Document |
|---|---|---|---|---|
| Product & Requirements | Core | `contract-grade` | `contract-grade` | [product-and-requirements.md](product-and-requirements.md) |
| Domain & Data | Core | `contract-grade` | `contract-grade` | [domain-and-data.md](domain-and-data.md) |
| Architecture | Core | `contract-grade` | `contract-grade` | [architecture.md](architecture.md) |
| Interfaces & Contracts | Core | `contract-grade` | `contract-grade` | [interfaces-and-contracts.md](interfaces-and-contracts.md) |
| Quality & Testing | Core | `contract-grade` | `contract-grade` | [quality-and-testing.md](quality-and-testing.md) |
| Delivery Process | Core | `contract-grade` | `contract-grade` | [delivery-process.md](delivery-process.md) |
| Security & Privacy | Baseline | `contract-grade` | `contract-grade` | [security-and-privacy.md](security-and-privacy.md) |
| Governance & Compliance | Baseline | `contract-grade` | `contract-grade` | [governance-and-compliance.md](governance-and-compliance.md) |
| User Experience | Module | `contract-grade` | `contract-grade` | [user-experience.md](user-experience.md) |
| Integrations & External Dependencies | Module | `contract-grade` | `contract-grade` | [integrations-and-external-dependencies.md](integrations-and-external-dependencies.md) |

## Concerns out of scope

| Concern | Kind | Justification |
|---|---|---|
| Operations & Infrastructure | `absent` | No deployed infrastructure exists — a local PC plugin and a device on the LAN. No re-entry note owed. |
| Observability & Monitoring | `deferred` | No diagnostics surface in v1 beyond the link screen and the configuration page's counters. **Re-entry:** per-component telemetry and a retained on-device log, once there is a reason to ask "what happened ten minutes ago". |
| Performance & Scalability | `deferred` | Operator decision, 2026-09-21. Success is judged in ordinary use rather than measured. **Re-entry** is written in [product-and-requirements.md](product-and-requirements.md) — one end-to-end latency target and its measurement method. |
| Accessibility & Internationalization | `deferred` (a11y) / `absent` (i18n) | Classic red/green palette chosen knowingly for v1. **Re-entry** is written in [user-experience.md](user-experience.md). Single locale, glyph-based UI — i18n has no subject. |
| Business & Legal | `deferred` | Non-commercial v1; its one live constraint was folded into Governance at intake. **Re-entry:** licence tiers, entitlements and EULA, if distribution ever becomes commercial. |

Dictum records three advisory **scope warnings** for this set — an interactive UI with
accessibility out, a real performance need with Performance out, and classified fields with
Governance's data-handling policy deferred. All are recorded in the manifest and block nothing.

## Build-readiness

**All ten in-scope concerns are at `contract-grade`, and all ten are `published`.** The set is
**build-ready** as of 2026-09-23.

Integrations was the last to move. It was audited twice on 2026-09-21 — corrected **down** to
`specified` after the first audit, raised back after research, and the second audit reversed that
raise — and then held at `specified` deliberately, because Delivery Process schedules confirming the
iRacing mapping as *build step 1*, and a claim that the mapping was finished would have contradicted
the playbook a builder actually follows.

That step was carried out on 2026-09-22 with a purpose-built probe plugin: 5,219 samples from a live
iRacing session, every row of the mapping table recorded. It corrected three answers that research
had got wrong, including the title-identity string. Integrations rose to `contract-grade` on the
evidence rather than on argument. Each round trip is recorded in that doc's *Rung note*.

**Published is a claim about the documents, not about the product.** Two checks were closed by
operator judgement rather than by the method they specify, and both say so where they live: `U5`'s
slow-motion flash-rate measurement was **waived**, and `X10` — both edge bars lighting when between
two cars — was **accepted without the live race it asks for**, on the strength of iRacing's published
SDK and independent corroboration of the derivation. Neither is recorded as verified, and `X10`
names the symptom to watch for if the acceptance turns out to be wrong.

**127 contract IDs** are minted, each on exactly one register line in its owning concern, with
every reference resolving and every contract carrying a coverage-map row — a named check or a
stated `n/a — why`.

## v1 scope: iRacing only

**v1 supports iRacing and nothing else** (decided 2026-09-21). Assetto Corsa, ACC and Euro Truck
Simulator 2 are v2. iRacing is the only title where all four elements can work, so it is the only
one that tests **the concept** rather than a subset of it. That also makes v1 the *complete*
product rather than a partial one.

### How titles are added

Each supported sim is **one source module implementing a common title-adapter interface**,
registered in one place, shipped in a single plugin assembly. The adapter resolves everything
sim-specific — including the shift-light fallback chain — and emits a normalised frame carrying
absolute RPM thresholds, so **the device carries no per-title logic at all**. Adding a title never
means reflashing the panel.

| Sim | Gear | Shift ramp & flash | Left bar | Right bar |
|---|---|---|---|---|
| iRacing — **v1** | yes | yes | yes | yes |
| Assetto Corsa / ACC — v2 | yes | yes | needs opponent-derived rule | needs opponent-derived rule |
| Euro Truck Simulator 2 — v2 | yes | scoped out | unavailable at source | unavailable at source |

## Two findings worth knowing before building

**iRacing supplies its own shift-light RPMs.** Under `GameRawData.SessionData.DriverInfo` the sim
publishes, per car, `DriverCarSLFirstRPM`, `DriverCarSLShiftRPM`, `DriverCarSLLastRPM`,
`DriverCarSLBlinkRPM` and `DriverCarRedLine` — absolute RPM, ascending in that order. The adapter
ramps First → Shift and flashes at Shift, using the car's own numbers rather than SimHub's computed
`CarSettings_RPMShiftLight*` fractions, which are documented as SimHub's approximation rather than
the car's lights.

**ArduinoJson must be 7.4.3 or later, as a security control.** Every version through 7.4.2 carries
a buffer overrun in string-to-float conversion, reachable by a JSON string of many digits. This
product's JSON parser is the only place an untrusted host on the LAN reaches its code, and the 2 KB
datagram cap does not close it. Recorded as `SEC-PARSER-FLOOR`.

## Remaining open items

~~No mapping row has been observed against a live session.~~ **Closed 2026-09-22** — every row
recorded from 5,219 live samples.

What remains is not documentation:

- **`X10` is accepted rather than verified.** Whether iRacing emits its both-sides spotter value in
  a real race is unobserved; the derivation around it is well corroborated. The symptom if it is
  wrong is named in Integrations.
- **`U5`'s flash-rate tolerance is unmeasured** by choice. The arithmetic is asserted in the unit
  tier; the panel is not measured against it.
- **The nine link-state icons are unbuilt.** `SCREEN-LINK` renders its plain-language lines only,
  and `U8`/`U13` wait on the icons.
- **`A1`, `A10` and the success criteria** need ordinary use and an unaided setup trial, neither of
  which is a doc task.

## Next step

**Build step 1** — confirm the iRacing mapping. It takes Integrations to Contract-grade and is the
first step of the build anyway, so nothing is spent twice. Publishing follows once all ten are at
target; no build markers remain, so that step is mechanical.
