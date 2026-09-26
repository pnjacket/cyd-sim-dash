---
artifact: product-doc
role: concern
concern-id: quality-and-testing
behavior: core
trigger: always
in-scope-subaspects: [test-pyramid-test-types, coverage-map, real-flow-e2e-standard, quality-bars-gates, test-data-strategy, specialized-testing, manual-exploratory]
current-rung: contract-grade
status: published
version: 0.12.0
---

# Quality & Testing — cyd-sim-dash

> How a display you can only judge by looking at it gets tested anyway — and what "tested" means for every contract in the set.

## Purpose & Scope

Two testability problems shape this concern. First, the output is **pixels on a physical panel**,
which no assertion library inspects. Second, the input is **a live racing session**, slow to produce
and nearly impossible to reproduce on demand — "two cars alongside on the left at the shift point"
is not a state you can drive to deliberately.

Both are now solved structurally rather than worked around. Splitting the display-state engine from
the renderer makes the logic ordinary testable code; exposing that state over `API-STATE` makes it
assertable from outside; and the replay tool makes any frame, valid or not, producible on demand.

## Non-goals / Out-of-scope

- **`test-stage-fidelity-mapping`** — *absent*. One environment: the rig. No staging, no CI
  environment tiers, nothing to map tests across. CI builds and runs the off-device tiers; it is
  not a second environment the product runs in. No re-entry note owed.
- **Performance testing** — *deferred* with its concern. No latency target exists to test against.
  [FUTURE-SCOPE] Re-entry: end-to-end latency from RPM crossing to pixels changing.
- **Accessibility testing** — *deferred* with its concern. [FUTURE-SCOPE] Re-entry: measured
  contrast across the ramp and colourblind simulation of the palette.
- **Pixel-level verification is not automated** — the state endpoint `API-STATE` proves the logic
  and the frame path, not what the glass shows. Colour, legibility, geometry and flash feel are proven by a named
  manual pass instead. Stated plainly rather than implied: **a change that renders correct state
  incorrectly is caught by a human or not at all.**

## Requirements

### Test pyramid / test types

| Tier | Runs | Covers |
|---|---|---|
| **Firmware compile** | CI, `arduino-cli` | The same sketch the documented Arduino IDE path builds, with core and library versions pinned in the workflow — so a green build also proves the recorded version table |
| **Unit — display-state engine** | CI, off-device | Frame in, display state out: ramp position, threshold crossing, gear glyph selection, unavailable elements, staleness, producer-restart. Where most logic lives |
| **Unit — title adapter** | CI, off-device | SimHub property values in, normalised frame out, including the fallback chain and every unavailable case |
| **Adapter conformance suite** | CI, off-device | The shared checks every adapter must pass, present and future — see *Contracts* |
| **Contract tests over the wire** | CI, both sides | Plugin and firmware validated against the same fixtures, so two independently-updated halves in different languages cannot drift apart silently |
| **Integration — plugin to device** | On-device | Real UDP, real registration, real device. Where the error catalogue's rows are forced |
| **Real-flow E2E** | On-device | A captured lap replayed end to end, asserted through `API-STATE` and the HTTP surfaces |
| **Manual seat-time** | On the rig | Everything about the glass: colour, legibility, geometry, flash feel, night comfort |

The fallback chain is tested at the **adapter** tier, not on the device — it moved to the PC with
the normalised-frame decision, and the tests moved with it.

### Coverage map

The full map is in *Contracts*: **every in-scope contract ID in the set**, each mapped either
to the check that proves it or to an explicit `n/a` row with its reason. Silent omission is the
defect; a stated `n/a` is not.

Checks are referenced by their owning concern's letter: **A** Product & Requirements, **D** Domain &
Data, **R** Architecture, **I** Interfaces, **U** User Experience, **X** Integrations,
**S** Security & Privacy, **G** Governance & Compliance, **V** Delivery Process, and **Q** this
concern.

### Real-flow E2E standard

The real-flow bar is minted as `E2E-STANDARD` in *Contracts*. Its shape here is unusual in one
respect worth naming: the
standard requires an E2E to exercise the system's **own** code paths for real while permitting
**external** dependencies to be substituted. The replay tool substitutes SimHub, the adapter and
the sim — all external or PC-side — while every line of firmware runs for real. That is a
legitimate substitution, not a bypass.

The requirement to **operate every interactive control** on each screen visited is nearly vacuous
here and is recorded as such: three of five screens have no controls at all, because the panel is
read-only. The two HTTP screens have real forms, and the E2E submits each in its default and empty
state.

### Quality bars & gates

- **CI runs on every push**, compiling the firmware with `arduino-cli`, building the plugin, and
  running every off-device tier.
- **An OTA push requires a green CI run.** OTA makes flashing a mounted panel a two-second
  operation, which is precisely why a gate exists in front of it.
- **Any change touching the renderer, the palette or screen geometry additionally requires the
  manual pass**, because no automated check covers the glass.
- **Flake policy: zero retries, no quarantine.** A red run is investigated, never re-run in hope.
  A genuine transient is recorded as an incident with both runs attached rather than added to a
  skip list — a quarantine list is how a flaky test becomes a permanently disabled one.
  Timing-sensitive rows are handled by the precondition rule in *Contracts*, not by retries.
- [GAP] No coverage percentage threshold is set. Thresholds on a codebase this small tend to
  measure diligence in writing tests for trivial code rather than risk; the coverage map is the
  real bar here. Recorded as a deliberate omission to revisit if the code grows.

### Test-data strategy

Two fixture kinds, both in scope (confirmed 2026-09-21):

- **Captured laps** — real telemetry, replayed for realism and regression. One iRacing capture
  covers v1. [FUTURE-SCOPE] Each v2 title needs its own, because a capture records normalised
  frames and therefore proves that title's adapter too.
- **Synthetic frames** — everything a lap cannot contain: malformed JSON, inverted thresholds,
  transposed stamps, bumped majors, every gear including two-character values, both bars at once,
  and each element as `null`. The error catalogue names the replay tool as the forcing mechanism for
  six of its fifteen rows; the rest are forced by DNS misconfiguration, disabling the access point,
  a bad NVS record, a stub adapter that throws, form submissions, a wrong credential, an unrouted
  HTTP path and port contention — so these fixtures are load-bearing rather than optional.

Captures carry nothing tagged `secret` or identifying, which is structural rather than a
discipline: a capture serialises frame fields only, and no such field appears in a frame.

### Specialized testing

Performance and accessibility testing are out of scope with their concerns. **Security testing is
in scope** and is specified as checks Q6–Q8 in *Contracts*, covering credential confinement, the
OTA and configuration-page gate, and malformed-input hardening — the last being the only path by
which an untrusted party on the LAN can reach a bug.

### Manual / exploratory

Not an admission of defeat but the **primary success measurement**: Product & Requirements judges
every success criterion this way, and no numeric target exists anywhere in the set.

What this concern owes is therefore a **record, not a procedure**. Sampling is the operator's call
and deliberately unprescribed. What is owed is somewhere each success criterion is marked met or
not met, so that results exist as evidence when the v2 gate is reached rather than as recollection.

The manual pass also carries what `API-STATE` cannot: colour, legibility, geometry, flash feel.

## Open Questions

- [GAP] No coverage threshold, deliberately — see Quality bars.
- [GAP] Where the manual record lives. Delivery Process owns the in-repo tracker and this record
  belongs beside it.
- Firmware CI — **resolved**; `arduino-cli`, pinned to the same core and library versions the
  documentation records, so a green build also proves that table.

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Product & Requirements | the capability and success registers the map spans, and the success criteria the manual pass records |
| Domain & Data | every invariant, each of which owes an assertion |
| Architecture | the components and patterns under test; the replay component that makes fixtures possible |
| Interfaces & Contracts | the wire contract the contract tests validate both sides against, and the error catalogue whose rows name their forcing mechanism |
| User Experience | the screens and journeys the E2E walks |
| Integrations | the dependency set and the substitution claim the E2E relies on |
| Delivery Process | consumes the gates below as Definition-of-Done items |

## Examples / Worked scenarios

**A regression caught by contract tests.** The plugin gains a field and its major is bumped by
mistake rather than its minor. The shared fixtures fail on the firmware side before anything is
flashed, because both halves validate against the same files.

**A regression the tests cannot catch.** The ramp's amber is changed to a shade that washes out
against the glyph's outline. Every automated check passes — the display state is correct, the
frames are correct. It is caught in the manual pass, or on track. This is the honest limit of the
chosen approach, and the reason renderer changes carry a manual gate.

**Forcing an error row.** `ERR-OUT-OF-ORDER` needs a frame whose stamp does not exceed the newest
seen. No lap produces one on demand; the replay tool transposes two lines of a capture and the
counter increments.

**A transient red run.** An integration test fails on real WiFi. Policy forbids re-running in hope,
so it is investigated. It proves genuinely transient; the incident and both runs are recorded, and
nothing is quarantined.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| Assert through the state endpoint rather than on pixels | Deterministic, needs no camera or golden images, and reuses a surface that is useful to humans anyway | Proves the logic and the frame path, never the glass. Pixel regressions are a manual responsibility |
| The endpoint ships in release builds | An endpoint compiled only into test builds would mean the E2E proves a binary nobody runs | One more always-listening surface, and a scope key reopened |
| CI on push, green required before OTA | OTA makes flashing a mounted panel trivial; the gate exists because the action is easy | CI setup work, including an unsolved firmware-build question |
| Zero retries, no quarantine | A quarantine list is how a flaky test becomes a permanently disabled one | A genuinely flaky environment costs investigation time rather than being silenced |
| No coverage percentage | On a codebase this small it would measure diligence on trivial code rather than risk; the coverage map is the real bar | Nothing mechanically prevents a new module arriving untested — the map must be maintained by hand |

## Contracts

### `E2E-STANDARD` — the real-flow bar

An end-to-end test satisfies this standard when all of the following hold:

1. It runs against **real firmware on real hardware**, over a real network.
2. Every line of first-party firmware executes for real. No code path is stubbed, skipped or
   short-circuited, and no state is injected past the wire.
3. Only **external and PC-side** dependencies are substituted — SimHub, the adapter and the sim,
   replaced by the replay tool at the wire contract. This is the approved substitution.
4. It asserts the **real rendered outcome** — as observable through `API-STATE` — not on internal
   variables reached by a debug hook.
5. It **operates every interactive control** on each screen it visits, in the control's default and
   empty state, asserting a non-error outcome. Three panel screens have no controls; the two HTTP
   screens have forms, and each is submitted.
6. Expected values may be computed in the test process by importing the product's own **pure**
   modules — the display-state engine is deterministic, so deriving an expectation from it is
   observation, not injection.

### Coverage map

Every in-scope contract ID. `n/a` rows state why no observable check exists.

**Capabilities**

| Covered by | Contract |
|---|---|
| A2 · D1 · U2 · U3 · E2E replay | `CAP-GEAR` |
| A3 · D12 · U4 · U5 · X2 · E2E replay | `CAP-SHIFT` |
| A4 · U7 · X2 · X3 · X10 | `CAP-SPOTTER-LEFT` |
| A4 · U7 · X2 · X3 · X10 | `CAP-SPOTTER-RIGHT` |
| A5 · A6 · U8 · I13 | `CAP-LINKSTATE` |
| A9 · I11 · I12 · U10 | `CAP-PROVISION` |
| A7 · U11 | `CAP-RECONFIG` |
| A8 · I4 · I7 · I8 · X1 | `CAP-PUBLISH` |

**Personas** — both `n/a`: a persona is a definition, not a behaviour. `PERSONA-ADOPTER` is
nonetheless *exercised* by A10, the unaided-setup attempt, which is the closest thing to a test of
a persona this set contains.

| Covered by | Contract |
|---|---|
| n/a — a definition; every operator-facing check exercises it implicitly | `PERSONA-OPERATOR` |
| n/a — as above; A10 exercises the assumptions it encodes | `PERSONA-ADOPTER` |

**Success criteria** — all seven are manual by declared method, recorded in the manual pass.

| Covered by | Contract |
|---|---|
| A1 manual pass | `SUCCESS-GEAR-GLANCE` |
| A1 manual pass | `SUCCESS-SHIFT-PERIPHERAL` |
| A1 manual pass | `SUCCESS-SPOTTER-NOTICED` |
| A1 manual pass | `SUCCESS-NO-DISTRACTION` |
| A1 manual pass | `SUCCESS-NIGHT-COMFORT` |
| A1 manual pass, minus its traced exception | `SUCCESS-LINK-DIAGNOSABLE` |
| A10 | `SUCCESS-SETUP-UNAIDED` |

**Entities**

| Covered by | Contract |
|---|---|
| I1 · I2 · I3 · D1 · contract-test fixtures | `ENTITY-FRAME` |
| X2 · X3 · X10 · D13 | `ENTITY-SPOTTERSTATE` |
| I13 · unit tier, display-state engine | `ENTITY-DISPLAYSTATE` |
| I7 · I8 · I9 | `ENTITY-REGISTRATION` |
| D7 · D8 · I11 | `ENTITY-DEVICECONFIG` |
| D12 | `ENTITY-GAMEPROFILE` |
| D9 · X7 | `ENTITY-CAPTURE` |

**Invariants** — one assertion each, so a failure attributes to exactly one invariant.

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| D1 | `INV-RAMP-ORDER` |  | D13 | `INV-STATUS-CONSISTENT` |
| D1 | `INV-RPM-NONNEG` |  | D7 | `INV-CONFIG-SINGLETON` |
| D1 | `INV-GEAR-DOMAIN` |  | D8 | `INV-CONFIG-MIGRATION` |
| D2 · D3 | `INV-STAMP-ORDER` |  | D9 | `INV-CAPTURE-CLEAN` |
| D4 | `INV-FRESH-RENDER` |  | D10 · Q6 — **advisory**, so the check bounds rather than proves it | `INV-SECRET-CONFINEMENT` |
| D5 · X3 | `INV-UNAVAILABLE-DISTINCT` |  | D6 · I6 | `INV-VERSION-WHOLE` |

**Components**

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| R2 · X5 | `COMPONENT-PLUGIN-CORE` |  | unit tier · I13 | `COMPONENT-STATE` |
| X1 · X2 · X4 · conformance suite | `COMPONENT-ADAPTER-IRACING` |  | R7 · U1 · U2 · manual pass | `COMPONENT-RENDER` |
| I4 · I7 · I8 | `COMPONENT-PUBLISHER` |  | I11 · I13 · I14 · A7 · A9 | `COMPONENT-WEB` |
| X7 · R10 | `COMPONENT-REPLAY` |  | D7 · D8 | `COMPONENT-CONFIG` |
| I2 · I3 · I6 · D2 · D3 | `COMPONENT-NET` |  |  |  |

**Patterns**

| Covered by | Contract |
|---|---|
| I10 — every catalogue row provoked, none console-only | `PATTERN-ERROR` |
| R4 · X5 | `PATTERN-FAULT-BOUNDARY` |
| R5 | `PATTERN-STATE-HANDOFF` |
| R6 · A6 | `PATTERN-RECONNECT` |
| n/a — a routing convention with no behaviour of its own; the never-console-only requirement it serves is proven by I10 | `PATTERN-LOGGING` |

**ADRs** — a decision is not itself testable, but most have a fitness consequence that is.

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| n/a — the approach the whole product embodies | `ADR-FIRST-PARTY-PLUGIN` |  | X8 | `ADR-ARDUINO-OTA` |
| an OTA performed while the panel is receiving live telemetry — the condition that produced the defect | `ADR-OTA-EXCLUSIVE` |  |  |  |
| I7 · I8 | `ADR-DEVICE-INITIATED` |  | n/a — a library choice; X8 proves its version is recorded | `ADR-TFT-ESPI` |
| I1 | `ADR-JSON-WIRE` |  | R5 | `ADR-TWO-TASKS` |
| R2 | `ADR-ADAPTER-MODULES` |  | R7 | `ADR-DIRTY-REGIONS` |
| R1 | `ADR-NORMALISED-FRAME` |  | R9 | `ADR-STD-LIBS` |
| R8 | `ADR-STATUS-FRAME` |  | R10 | `ADR-REPLAY-STANDALONE` |

**Interface elements** — each event element covered for delivery, ordering and its authorization
posture; the API element for its response projection and its vacuous input case.

| Covered by | Contract |
|---|---|
| delivery I7 · idempotency I8 · identifier validation I9 · **authorization** Q5, which asserts the contracted *absence* of authorization rather than assuming it | `EVT-REGISTRATION` |
| delivery and projection I1 · unavailable semantics I2 · I3 · cadence I4 · versioning I5 · I6 · ordering D2 · D3 · **authorization** S2, asserting the contracted absence of any source check | `EVT-FRAME` |
| I13 · I14. Input edge cases `n/a` — the element takes no parameters, so there is no default, empty or malformed input to exercise | `API-STATE` |
| X7 · D9 — a capture round-trips through the replay tool and carries no classified field | `OUT-CAPTURE` |

**Error catalogue** — all fourteen rows are provoked by I10 collectively; each also has its own
named forcing in the catalogue itself.

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| I6 · I10 | `ERR-VERSION-MAJOR` |  | R8 · I10 | `ERR-UNSUPPORTED-TITLE` |
| I10 | `ERR-MALFORMED` |  | R4 · X5 · I10 | `ERR-ADAPTER-FAULT` |
| D1 · I10 | `ERR-FIELD-RANGE` |  | I11 · I10 | `ERR-PORTAL-INPUT` |
| D2 · I10 | `ERR-OUT-OF-ORDER` |  | I12 · I10 | `ERR-PORTAL-VERIFY` |
| D4 · A5 · I10 | `ERR-LINK-SILENT` |  | U11 · I10 | `ERR-AUTH-FAILED` |
| I14 · I10 | `ERR-UNKNOWN-PATH` |
| n/a on this hardware — `S8` establishes that nothing above 1472 bytes reaches the application, so the condition cannot be provoked here; the counter is asserted by the unit tier instead | `ERR-OVERSIZED` |
| I10 · U8 | `ERR-HOST-UNRESOLVED` |  | I10 — **PC-side surface only**; the device cannot observe it, which the catalogue records as a known limitation | `ERR-PUBLISH-FAILED` |
| R6 · I10 | `ERR-WIFI-LOST` |  | D8 · I10 | `ERR-CONFIG-UNREADABLE` |

**Data-entry surfaces** — each field at default, empty and malformed.

| Covered by | Contract |
|---|---|
| I11 empty and malformed per field · I12 verification failure per step | `UIF-PORTAL` |
| I11 including the labelled blank-secret exception · A7 · U11 | `UIF-CONFIG` |

**Screens and journeys**

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| U9 | `SCREEN-BOOT` |  | U10 · A10 | `JOURNEY-FIRST-SETUP` |
| U1–U7 · U12 | `SCREEN-DRIVING` |  | U10 · E2E replay | `JOURNEY-SESSION` |
| U8 | `SCREEN-LINK` |  | U10 · R6 · A6 | `JOURNEY-RECOVERY` |
| I11 · I12 · A9 | `SCREEN-PORTAL` |  | U10 · A7 | `JOURNEY-RECONFIG` |
| U11 · A7 | `SCREEN-CONFIG` |  | U10 · I6 | `JOURNEY-VERSION-MISMATCH` |
| Q12 · Q13 · Q14 · `SUCCESS-DARK-WHEN-IDLE` | `CAP-BLANK` |  | Q12 | `SUCCESS-DARK-WHEN-IDLE` |
| Q16 · Q18 · Q20 | `CAP-WAKE-RIG` |  | Q20 | `SUCCESS-WAKE-FROM-PANEL` |
| Q17 | `INV-WAKE-NEEDS-LEARNED-MAC` |  | Q19 | `ENTITY-RIGADDRESS` |
| Q16 | `EVT-WAKE` |  | Q16 · Q18 | `COMPONENT-TOUCH` |
| S13 | `SEC-WAKE-PHYSICAL-ONLY` |  | n/a — a wiring decision, proven by touch working at all | `ADR-TOUCH-OWN-BUS` |
| Q15 · D8 | `INV-BLANK-BOUND` |  |  |  |
| A9 · U10 | `SCREEN-SETUP` |  | manual pass during an OTA | `SCREEN-UPDATE` |

**Security assertions** — mostly negative, and a negative assertion still owes a check: asserting
the contracted *absence* of a control is what makes adding one later a deliberate change.

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| n/a — a posture, not a behaviour; its consequences are proven by S2 and S3 | `SEC-LAN-TRUSTED` |  | n/a — a negative: nothing verifies the image, so there is nothing to observe | `SEC-OTA-IMAGE` |
| S1 | `SEC-NO-EGRESS` |  | S7, first dump | `SEC-STORAGE-PLAIN` |
| n/a — no personal data flow exists to observe; established by review of what is collected | `SEC-NO-PERSONAL-DATA` |  | S7, second dump | `SEC-ERASE-OVERWRITE` |
| S2 · Q5 | `SEC-NO-SUBSCRIPTION-AUTHZ` |  | S8 · S9 · Q8 | `SEC-INPUT-BOUND` | | S12 | `SEC-PARSER-FLOOR` |
| S2 | `SEC-NO-SOURCE-CHECK` |  | S1 | `SEC-TRANSIT-CLEAR` |
| S3 · I13 | `SEC-STATE-OPEN` |  | S7 · S10 | `SEC-FIELD-WIFI` |
| S4 | `SEC-CREDENTIAL-POLICY` |  | S6 · S7 · S10 | `SEC-FIELD-CREDENTIAL` |
| S5 | `SEC-OTA-WINDOW` |  | S11 · D9 | `SEC-FIELD-IDENTIFYING` |

**This concern's own contracts** — omitted from the first draft of this map, which is precisely the
failure `Q1` exists to catch.

| Covered by | Contract |
|---|---|
| Q2 — an E2E run is asserted against every clause | `E2E-STANDARD` |
| Q10 — each of the seven checks failed deliberately by a stub adapter | `CONFORMANCE-ADAPTER` |

**Governance policies** — each proven by a Governance check; one exercises its own failure path.

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| G1 | `POLICY-OUTBOUND-MIT` |  | G4, checked in both directions | `POLICY-PROVENANCE-ATTESTATION` |
| G6 | `POLICY-NO-COPYLEFT-SOURCE` |  | G5 | `POLICY-PROVENANCE-DETECTION` |
| G2 | `POLICY-CLEARANCE-GATES-SELECTION` |  | G6, by planting a unit rather than assuming the path works | `POLICY-INCOMPATIBLE-IS-DEFECT` |
| G3 | `POLICY-NO-REDISTRIBUTION-SIMHUB` |  |  |  |

**Dependencies**

| Covered by | Contract |  | Covered by | Contract |
|---|---|---|---|---|
| X6 · X9 | `DEP-SIMHUB` |  | X8 · manual pass | `DEP-TFT-ESPI` |
| X5 | `DEP-SIMHUB-SDK` |  | I10 malformed-input rows · X8 | `DEP-ARDUINOJSON` |
| X1 · X2 · X6 | `DEP-IRACING` |  | A9 · X8 | `DEP-WIFIMANAGER` |
| X8 | `DEP-ESP32-CORE` |  | X7 | `DEP-PYTHON` |

### `CONFORMANCE-ADAPTER` — the adapter conformance suite

`CONFORMANCE-*` is a **locally-coined prefix**, owned by this concern, which the standard permits:
it resolves because it is defined here and referenced consistently across the set.

Every adapter, present and future, passes the same checks. This is what makes adding a v2 title
cheap rather than merely possible. It carries an ID because three other concerns rely on it as an
enforcement mechanism, and a contract other concerns lean on should be referenceable rather than
named in prose.

| # | Check |
|---|---|
| C1 | Declares exactly one title identity, and the plugin core selects it for that title and no other |
| C2 | Never emits an inverted or equal threshold pair |
| C3 | Never emits a value for an element the title cannot source, and never coerces unavailable to a negative value |
| C4 | Emits only frames the wire contract accepts, validated against the shared fixtures |
| C5 | Runs the fallback chain in the specified order, and reports shift unavailable rather than inventing thresholds when no usable input exists |
| C6 | A missing or renamed source property degrades exactly that element, leaving all others intact |
| C7 | Never throws out of its own boundary for input the title legitimately produces |

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| Q1 | Every contract ID in the set appears exactly once in the coverage map, with either a check or a stated `n/a` reason; a newly minted ID with no row fails this check | coverage-map completeness |
| Q2 | An E2E run satisfies every clause of `E2E-STANDARD`, including operating both HTTP forms in their empty state | `E2E-STANDARD` |
| Q3 | A push with a failing test does not produce a green run, and an OTA attempted against a red run is refused or, if performed manually, recorded as a gate breach | the OTA gate |
| Q4 | A renderer or palette change without a recorded manual pass fails the Definition of Done | the manual gate |
| Q5 | An unregistered host on the LAN sends a registration and receives frames — asserting the contracted absence of subscription authorization, so that adding authorization later is a deliberate contract change rather than a silent one | `EVT-REGISTRATION` authorization posture |
| Q6 | A full provisioning and driving cycle is inspected across serial output, every screen, `API-STATE` and a capture file; no secret appears in any of them | `INV-SECRET-CONFINEMENT`, advisory |
| Q7 | The configuration page and the OTA path each reject an incorrect credential and an empty one | the shared-credential gate |
| Q8 | The device is sent a corpus of malformed and hostile datagrams — truncated, oversized, wrong types, deeply nested, missing keys — and neither crashes nor reboots, with every one counted | malformed-input hardening |
| Q9 | A test is failed deliberately, re-run without code change, and the resulting record shows an incident with both runs rather than a quarantine entry | flake policy |
| Q10 | Each of the seven conformance checks is failed deliberately by a stub adapter, and each failure is caught | adapter conformance suite |
| Q11 | Timing-sensitive rows — flash rate, staleness threshold — state their environment precondition and are not retried | flake policy, measurement rows |
| Q12 | The panel blanks after the configured period in a non-driving state, and **lights within one frame interval of a live frame being accepted**. The wake half is asserted separately and first: a panel that blanks and never wakes is indistinguishable from a dead one, and is the failure this feature can actually cause | `CAP-BLANK` |
| Q13 | With `blankAfterMinutes` set to 0 the panel never blanks, however long it is left in a non-driving state | `CAP-BLANK`, its disable path |
| Q14 | `SCREEN-UPDATE` and the `versionMismatch` condition stay lit past the configured period | the two stated exemptions |
| Q15 | A configuration record written at schema 1 is migrated and gains `blankAfterMinutes` at its default; host and credential survive | `INV-CONFIG-MIGRATION` against its first real migration |
| Q16 | From `unreachable` with the panel blanked, the **first** touch lights the panel and offers the wake and sends nothing — verified by watching the wire, not just the glass. The **second** emits exactly one `EVT-WAKE` of 102 octets whose payload is six `0xFF` followed by the learned address sixteen times | `CAP-WAKE-RIG`, and the two-touch rule |
| Q17 | With no learned address, a touch lights the panel and offers **nothing**, and no packet is sent | `INV-WAKE-NEEDS-LEARNED-MAC` |
| Q18 | In every link state other than `unreachable`, a touch lights the panel and arms nothing | the armed-state rule |
| Q19 | The address is learned while the PC is reachable and survives the reboot that happens when the rig powers down — asserted by power-cycling the rig, not by writing the value directly | `ENTITY-RIGADDRESS` |
| Q20 | **Against the real rig**: two touches start it and the panel reaches the driving screen unaided. This is the only check that can fail for reasons outside this product — Wake-on-LAN disabled in the rig's BIOS, fast startup defeating S5, or the adapter not retaining power — and a failure is triaged against the rig before it is triaged against the firmware | `SUCCESS-WAKE-FROM-PANEL` |

