---
artifact: product-doc
role: concern
concern-id: delivery-process
behavior: core
trigger: always
in-scope-subaspects: [vertical-slice-rule-slice-types, definition-of-done, build-playbook-sequence, work-item-hierarchy-slice-level, build-ready-gate-scope, verified-build-status-tracking, branching-release-versioning]
current-rung: contract-grade
status: draft
version: 0.4.0
---

# Delivery Process — cyd-sim-dash

> How this gets built in slices that each end with something visible, and what "done" has to mean for the other nine concerns to hold.

## Purpose & Scope

The build order, what "done" means for a slice, how releases are cut across three
independently-versioned things, and the record of what has actually been built and proven.

This concern is **reflexive**: it defines *Verified* for every other concern in the set. A capability
is not verified because its doc says so — it is verified when this process says it is.

A two-component product with a wire contract between them has an obvious hazard — building both
halves in parallel against an imagined contract — and the slice rule exists mainly to prevent it.

## Non-goals / Out-of-scope

- **`external-tracker-binding`** — *absent*. No Jira, no Azure DevOps, no GitHub Issues mirrors this
  build; the in-repo record **is** the tracker, which the standard explicitly permits. [REVISIT]
  Publishing v1 makes GitHub Issues the natural place for adopter demand and defect reports. The
  trigger is an adopter actually filing something, not the repository becoming public — and taking
  it means specifying an ID-carrying channel, a repo→tracker projection direction, and a repo-wins
  reconciliation rule.
- **Separate release infrastructure** — *absent*. There is no deploy pipeline and no environment to
  promote through; Operations is out of scope for the same reason. The consequence for the staged
  Definition of Done is stated below rather than left as a dangling reference.

## Requirements

### Vertical-slice rule + slice types

A slice ends with **something observable on the panel, or a passing contract test** — never with a
layer. "The plugin now reads properties" is not a slice; "the gear appears on the panel, driven by
live telemetry" is.

Three slice types are first-class, and none is a lesser citizen:

- **`slice:full`** — ends in something visible on the glass.
- **`slice:headless`** — a foundation layer a later journey rides on. The wire contract is the
  archetype: built first, proven by contract tests on both sides, because everything else depends
  on the two halves agreeing. Legitimately precedes the first visible slice.
- **verification-only** — a slice whose increment is *proof* over an already-built surface: the
  adapter conformance suite, the malformed-input battery, the provenance detection pass.
- **cross-cutting / infrastructure** — a slice whose increment is a shared capability several later
  slices ride on, and which realises no capability of its own: the repository skeleton, CI, the
  in-repo records, a release. The standard treats this as first-class; omitting it from this list
  was the structural reason the playbook below had no home for CI, the records or the release
  (found by the implementation planner, 2026-09-21).

**A slice is not one capability.** Several slices are cross-cutting — the wire contract serves four
capabilities at once; the link-state work serves one but touches every screen. No `CAP` bijection is
forced, and no slice IDs are minted: slices are process artefacts, not contracts.

The "no deferred E2E" rule is read at **capability granularity**: no capability completes without
its real-flow E2E, but headless foundation slices may land before the first journey exists.

### Definition of Done

**One gate, not two** — and that needs justifying rather than assuming. The standard's staged DoD
separates a merge gate (own code real, externals substituted) from a release gate (real
infrastructure). Here the developer stage and the real stage are **the same rig**: there is no
separate environment to promote into. The gates therefore coincide, and the full bar applies to
every slice.

**The substitution set is stated, not omitted**, because "nothing is substituted" would be false:
the replay tool substitutes SimHub, the adapter and the sim at the wire contract. That substitution
is owned by Integrations — Operations, which normally owns the fidelity map, is out of scope, so
the reference points there instead of at a document that does not exist.

A slice is done when **all** of the following hold:

1. Its contracts are implemented on both sides where applicable.
2. Its tests pass in CI — every off-device tier green, **and the firmware compiles** under
   `arduino-cli` with the pinned core and library versions.
3. The **binding map** links the contract IDs it touched to the code implementing them.
4. **Anything touching the glass** — renderer, palette, geometry, flash behaviour — has had a
   manual pass on the rig. No automated check covers the panel, so this is not ceremony.
5. **Newly adapted code carries its `SOURCE:` marker** and a register entry, per the provenance
   policy. Cheapest at the moment the code is written and close to impossible later.
6. **A slice touching any `secret`-tagged field** — provisioning, the receive path, the
   configuration page — has had its serial output, screens and any capture inspected for leakage
   before it lands. The confinement invariant is advisory and no schema enforces it; a single
   end-of-build sweep finds what is still there, not what was logged three slices ago.
7. The build-status record is updated.

Items 4, 5 and 6 exist because other concerns depend on them: without 4, User Experience's contracts
are unproven; without 5, Governance's register is reconstructed after the fact, which is the one
thing it cannot survive; without 6, the confinement invariant has no enforcement anywhere in the
process, only a sweep at the end.

### Build playbook / sequence

Dependency-ordered. Three revisions were made after the implementation planner walked it
(2026-09-21): the missing infrastructure, configuration-page, security-verification and release
steps were added, and the state endpoint moved earlier. Its reasoning is recorded with each.

1. **Confirm the iRacing data mapping** live in SimHub's property picker: gear, RPM, the
   `DriverCarSL*` shift points, both spotter states, and the game-identity property. The mapping is
   already authored; what closes here is every unconfirmed row in Integrations, which is what takes
   that concern from `specified` to Contract-grade. **Needs the rig.**
2. **Repository, CI and the records.** *Cross-cutting.* Repository skeleton, the `arduino-cli` and
   plugin CI workflow with versions pinned, the in-repo tracker, the build-status record, an empty
   binding map, the third-party notice file and the provenance register. **Added**: Definition-of-Done
   items 2, 3 and 5 all bite from the first slice onward, so none of this can arrive later without
   the DoD being unmeetable in the meantime.
3. **The wire contract**, with fixtures both sides test against. Cross-cutting, headless, first of
   the product slices.
4. **The display-state engine**, whole, as a pure host-compilable module with its unit tier.
   **Moved earlier**: Delivery's earlier `[REVISIT]` suggested splitting each display slice into a
   state half and a render half; that would build the same pure module three times. One engine
   ahead of all three display slices is strictly better, and the render slices then really are thin.
5. **Plugin core, the title-adapter interface, the iRacing adapter, and the conformance suite.**
   Sketch the ETS2 adapter on paper before calling the interface done — a source with *no* proximity
   data is what it is most likely to fail to express. Blocked on step 1.
6. **Publisher, device table and capture writer.** Enables the capture sitting. **Needs the rig** to
   record a lap.
7. **Replay tool and synthetic fixture generator.** Early, because six error-catalogue rows name it
   as their forcing mechanism and `E2E-STANDARD` makes it the approved substitution.
   **It must operate with no captured lap present** — otherwise every firmware slice below stalls
   behind rig availability.
8. **Device bring-up: panel and boot screen.** The TFT_eSPI pin configuration lands here, which is
   the most likely single cause of a blank panel.
9. **Configuration store and provisioning portal.**
10. **Receive path, registration, cross-core handoff, `API-STATE`, and the E2E harness.**
    **Moved earlier** (was step 10 on its own): `E2E-STANDARD` asserts through the state endpoint,
    so every display capability below would otherwise complete with its real-flow E2E unsatisfiable
    — which the slice rule, read at capability granularity, forbids.
11. **Gear indication.** The first slice that is visibly the product.
12. **Shift ramp and flash.** Before the bars, because the composition rule paints bars over the
    flash — they cannot be proven correct until the flash exists.
13. **Edge bars and the composition rule.**
14. **Link-state screen**, its nine conditions, icons and lines.
15. **Configuration page, reconfiguration and erase.** **Added**: `CAP-RECONFIG` is in the gate
    scope and had no step. It also carries the entire soft-fault half of `PATTERN-ERROR` — all four
    counters surface there and nowhere else — and the control that opens the OTA window, so the OTA
    step was silently blocked on it.
16. **OTA and the update window.**
17. **Security and error-catalogue verification.** *Verification-only.* The malformed-input battery,
    the egress capture, the flash-dump pair, and every error row provoked by its named forcing.
    **Added**: Delivery names this as its archetype verification-only slice but scheduled no step
    for it. Note the split: `SEC-INPUT-BOUND` is a *mechanism* that lands with the receive path in
    step 10 and cannot be retrofitted; this step is the *battery* that proves it.
18. **`E2E-STANDARD` conformance and the five journeys.**
19. **Live iRacing integration.** **Needs the rig.**
20. **Documentation, notice file, provenance detection pass, and the v1 release.** Manuals last.
    **Added the release**: four checks assert against release artefacts and no step cut one. The
    provenance pass runs before the first public push, and a mismatched firmware/plugin pair is a
    real artefact that must be built for the version-mismatch check.
21. **Adopter unaided-setup trial.** Needs a willing volunteer.
22. **Drive it**, recording each success criterion as met or not met. **Needs the rig.**
23. **The v2 Go/No-Go gate.**

**Rig dependency is the critical path, and it is three sittings rather than the one the earlier
playbook implied**: the mapping (step 1), the lap capture (step 6) and live integration (step 19).
Steps 2, 3, 4, 7, 8, 9 and 10 need no rig at all, so the firmware track can run well ahead of rig
availability provided step 7 honours its synthetic-only condition.

~~[REVISIT] Steps 6–8 could each split into a display-state slice and a render slice.~~
**Resolved 2026-09-21, the other way**: one display-state-engine slice ahead of all three (step 4),
rather than three state/render pairs that would build the same pure module three times.

### Test-harness composition

Recorded here because it is a delivery-shaped decision that the test standard assumes and no
concern stated. `E2E-STANDARD` clause 6 permits a test process to import the product's **pure**
modules to compute expectations. The display-state engine is C++ and the replay tool is Python, and
no glue between them exists or is wanted. The resolution:

- The engine is written as a **platform-independent C++ module, compiled for the host**, and its
  unit tier is C++ — that is where clause 6's import applies, natively and without glue.
- The **end-to-end harness is Python** and asserts **only through `API-STATE` over HTTP**. It
  imports nothing from the firmware, so clause 6 simply does not apply to it.

Neither half needs a binding to the other, which is why no `ctypes` or `pybind` layer appears
anywhere in this plan.

### Work-item hierarchy & slice level

**The in-repo record is the tracker.** A markdown file listing slices, each with the contract IDs it
touches and its status. The **slice is the work-item level** — there is no epic above it and no task
below it, because a one-person build gains nothing from either.

An item references contract IDs; it never restates what a contract says. If a slice's description
and a contract disagree, the contract wins and the item is wrong.

### Build-ready gate scope

The gate is per Dictum: every in-scope concern at Contract-grade, the set published, no
unreconciled staleness.

**v1's gate scope is the whole product, for iRacing** — all eight capabilities, enumerated so the
gate is checkable rather than descriptive: `CAP-GEAR`, `CAP-SHIFT`, `CAP-SPOTTER-LEFT`,
`CAP-SPOTTER-RIGHT`, `CAP-LINKSTATE`, `CAP-PROVISION`, `CAP-RECONFIG`, `CAP-PUBLISH`. A gate
covering a subset would not test the concept, and testing the concept is what v1 is for.

[FUTURE-SCOPE] Each v2 title is then its own gate scope: one adapter, its mapping table, its
conformance run, its capture. Nothing already gated is reopened, which is the delivery payoff of
the adapter design.

### Verified / build-status tracking

A separate in-repo record, deliberately **not** the manifest: doc maturity and implementation
status are different things and conflating them is how a set starts claiming a product that does
not exist.

Per capability and per slice: built, verified, and at which stage — off-device (CI tiers green) or
on-rig (manual pass done). `dictum/templates/build-status.template.md` gives the shape.

### Branching / release / versioning

Three things version independently, and the release scheme exists to stop that becoming a trap:

- **Firmware and plugin share one repository version**, released together under one tag. A release
  therefore means *these two halves were tested together* — which is the only claim worth making,
  given a user can update one and forget the other.
- **The wire protocol versions separately**, on its own major/minor, because its whole purpose is to
  change more slowly than the code.
- A plugin-only fix still bumps both halves. Accepted: the cost is a version number, and the
  benefit is that no released pair is ever untested.
- Each release records the protocol version it speaks, the tested SimHub version, and the tested
  library versions — the three facts an adopter needs and cannot otherwise discover.

**On a v2 No-Go:** the repository is marked unmaintained with a notice stating plainly that it is
iRacing-only and finished, and **the last release is left in place and working**. Someone who
adopted it keeps a working device and knows not to expect fixes. That is one commit, and it is what
a stranger actually needs.

## Open Questions

- Firmware CI — **resolved**; `arduino-cli` builds the same sketch the documented Arduino IDE path
  builds, with the core and library versions pinned in the workflow. That choice does a second job:
  the workflow's pins and the version table Integrations owes are the same facts, so a green build
  proves the table is right rather than merely written down.
- [GAP] The binding map does not exist yet. `dictum/templates/binding-map.template.md` gives the
  shape; it is created with the first slice, since DoD item 3 references it from the outset.

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Quality & Testing | the real-flow E2E standard the DoD's proof-of-done references, the CI gates, and the manual record |
| Product & Requirements | the capability register slices realise, and the success criteria step 13 records |
| Architecture | the components slices map to, and the adapter boundary the playbook protects |
| Interfaces & Contracts | the wire contract that is the first slice, and the protocol version the release scheme tracks |
| Integrations | the substitution set the Definition of Done names, and the version facts each release records |
| Governance & Compliance | the provenance marker check in the DoD, and the notice file in step 12 |
| User Experience | the journeys the E2E walks, and the screens the manual pass judges |

## Examples / Worked scenarios

**A headless slice.** The wire contract lands with fixtures on both sides. Nothing is visible on any
panel. It is nonetheless a complete slice: its increment is two independently-built halves that
provably agree, and four capabilities ride on it afterwards.

**A slice that fails the DoD.** The ramp's colours are adjusted and CI is green. The slice is not
done: it touched the glass and no manual pass was recorded. The gate catches exactly the class of
change no automated check covers.

**A verification-only slice.** The malformed-input battery is written against firmware that already
exists. It adds no capability and changes no behaviour. It is a first-class slice, because its
increment is proof of a security assertion that was previously only asserted.

**A release.** Firmware and plugin are tagged together at one version. The release notes record the
protocol version, the tested SimHub version and the tested library versions. An adopter updating
later knows which pair was tested together.

**A No-Go.** The seat-time record shows the concept did not earn its place. The operator decides
against v2. A notice goes on the repository; the last release stays up and keeps working for
whoever is running it.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| One DoD gate, not two | The developer stage and the real stage are the same rig; there is no environment to promote through | The full bar applies to every slice, including the manual pass where relevant |
| Manual pass and provenance marker are DoD items | Other concerns depend on both; a check nobody is obliged to run is not a check | Heavier per slice, deliberately |
| In-repo markdown tracker, slice as the item level | The standard permits it, and a one-person build gains nothing from a hierarchy | Adopter demand has no home until the tracker question is revisited |
| Firmware and plugin share one version and one tag | A release then means "tested together", which is the claim that matters when either can be updated alone | A plugin-only fix bumps both |
| Protocol versioned separately | It changes more slowly than the code, by design | One more number to reason about |
| No-Go leaves the last release working | An adopter keeps a working device and learns the truth; costs one commit | The repository stays up unmaintained rather than being tidied away |

## Contracts

Per the document contract, this concern's owned material is table- and prose-shaped and is fully
stated in *Requirements*. This section is a **pointer, not a re-listing**, and no `SLICE-###` prefix
is invented to populate it — slices are process artefacts, not contracts.

| Owned contract | Where it is stated |
|---|---|
| Slice rule and slice types | *Vertical-slice rule + slice types* |
| Definition of Done, with its substitution set | *Definition of Done* |
| Build playbook | *Build playbook / sequence* |
| Work-item hierarchy and declared slice level | *Work-item hierarchy & slice level* |
| Build-ready gate scope | *Build-ready gate scope* |
| Build-status record method | *Verified / build-status tracking* |
| Release and versioning scheme | *Branching / release / versioning* |
| Tracker-binding declaration | Not owed — no external tracker; see Non-goals |

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| V1 | A slice touching the renderer with green CI but no recorded manual pass is rejected as not done | DoD item 4 |
| V2 | A slice adding an adapted unit without a `SOURCE:` marker and register entry is rejected as not done | DoD item 5 |
| V3 | Every slice in the in-repo record references at least one contract ID, and no slice description restates a contract's content | work-item hierarchy |
| V4 | The build-status record contains no documentation rung, and the manifest contains no build or verification state — inspected field by field | doc maturity and implementation status are tracked separately |
| V5 | A release tag contains both halves, and its notes record the protocol version, the tested SimHub version and the tested library versions | release scheme |
| V6 | Firmware and plugin from the same tag interoperate; firmware from one tag and plugin from another with a different protocol major produce the version-mismatch screen rather than silent misbehaviour | the versioning scheme's purpose |
| V7 | The build-ready gate is evaluated and reports not-ready while any in-scope concern is below Contract-grade or any doc remains in draft | gate scope |
| V8 | Every capability in the register appears in the build-status record with a stage, or is explicitly recorded as not yet started | build-status completeness |
| V9 | Step 1 of the playbook closes every unconfirmed row in the Integrations mapping table before step 5, the adapter slice, begins | playbook ordering, where the real dependency is |
| V10 | A deliberate compile error in the firmware produces a red CI run, and the versions the workflow pins match the versions the documentation records | firmware CI, and the version table self-verifying |

