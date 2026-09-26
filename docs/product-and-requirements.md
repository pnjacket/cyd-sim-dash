---
artifact: product-doc
role: concern
concern-id: product-and-requirements
behavior: core
trigger: always
in-scope-subaspects: [problem-motivation, target-users-personas, goals-success-criteria, capability-register, constraints-assumptions, risks]
current-rung: contract-grade
status: published
version: 0.8.0
---

# Product & Requirements — cyd-sim-dash

> A glanceable four-element sim-racing dashboard on an ESP32 Cheap Yellow Display, fed over WiFi by a purpose-built SimHub plugin. v1 supports iRacing only.

## Purpose & Scope

A small dedicated display mounted above the wheel that answers four questions without the driver
looking away from the track: **what gear am I in**, **should I shift now**, **is there a car on my
left**, **is there a car on my right**. All four render simultaneously on one 320x240 panel.

Telemetry originates in SimHub on the sim PC and reaches the device over WiFi as UDP datagrams.
The product is therefore **two first-party components plus the wire contract between them**: a
SimHub plugin on the PC and firmware on the device.

This concern owns the capability register, the personas and the product success criteria that the
other nine concerns trace against. It does not own *how* anything is built, nor any non-functional
target.

## Non-goals / Out-of-scope

### Scoped-out sub-aspects

- **`market-competitive-context`** — *absent*. A personal build with no market position to
  establish; commercial dashboard products are not being competed with. No re-entry note owed.
- **`stakeholders-decision-makers`** — *absent*. One operator is author, decision-maker and user.
  Formalising a stakeholder map would document a single person. No re-entry note owed.

### Scoped-out product surface

- **v1 supports iRacing only** — *deferred*. Assetto Corsa, ACC and Euro Truck Simulator 2 are
  pushed to v2. The rationale is worth preserving: iRacing is the only title where all four
  elements can work, so it is the only one that tests **the concept** rather than a subset of it.
  If the concept disappoints, no effort will have been spent on titles that could never have shown
  it at its best. [FUTURE-SCOPE] Re-entry per title is one adapter module, plus — for AC/ACC
  proximity — a distance and lateral-offset rule computed from SimHub's opponent collection. ETS2
  needs no proximity work: the data does not exist at source, so its adapter reports proximity
  unavailable.
- **Not a full telemetry dash.** No speed, lap times, delta, fuel, tyre temperatures, position or
  flags. Four elements, deliberately. Adding a fifth is a change event, not a free extension.
- **Not a replacement for the in-game HUD.** It supplements what the sim already shows.
- **No control input over the sim.** The device never commands the sim, the car, or the session, and
  nothing it sends can affect a lap. `CAP-WAKE-RIG` is the one exception to the *device sends
  nothing* reading of this principle and deliberately not an exception to the principle itself: a
  Wake-on-LAN packet reaches the rig's network adapter while the machine is off, and a machine that
  is off is not running a sim. The narrower rule this now states is what was always meant. The
  touchscreen was unused
  in v1.
- **No per-title behaviour on the device, ever.** A consequence of the adapter constraint rather
  than a separate decision, and load-bearing enough to state as a non-goal: the firmware contains
  no sim-specific branch, so adding a title never means reflashing the panel.
- **Capture & replay is development tooling, not a product capability** — *deferred*. It lives in
  the repository and Quality & Testing depends on it, but it is not documented for end users, owns
  no user-facing capability, and its file format carries no stability commitment.
  [FUTURE-SCOPE] Re-entry if users are ever asked to attach captures to bug reports.
- ~~**No standby or blanking mode** — deferred.~~ **In scope from 2026-09-23** as `CAP-BLANK`. The
  reasoning that deferred it was wrong on its facts: it assumed "a mounted device is rarely powered
  independently of the rig", and this one is — observed 2026-09-26 with the panel up for 7.6 minutes
  while the rig was unreachable. That same observation is what makes `CAP-WAKE-RIG` possible at all,
  since a panel that died with the rig would have nothing to touch.

### Scoped-out measurement

- **No numeric latency target exists, by decision.** Success is judged in ordinary use by the
  operator rather than by instrumentation, and Performance & Scalability remains out of scope. The cost is stated plainly
  rather than buried: **a shift flash that is slow but still tolerable will not be caught**,
  because nothing measures it. Accepted knowingly (operator decision, 2026-09-21).
  [FUTURE-SCOPE] Re-entry is a minimal Performance concern owning one end-to-end latency target
  and its measurement method.
- **This concern references no non-functional target**, so the Contract-grade requirement that
  referenced NFR targets resolve to a home-concern owner is satisfied vacuously. That is a
  consequence of the decision above, and it is recorded here so a later reader does not mistake
  the absence for an oversight.

## Requirements

### Problem & motivation

Gear and shift timing live in the driver's peripheral vision inside the sim, competing with the
track for attention. A dedicated panel at a fixed spot above the wheel puts the four highest-value
signals somewhere the eye can find them without re-focusing. Proximity awareness in particular is
poorly served by in-game mirrors during close racing, where the decision to hold a line or yield
is made in well under a second.

The product exists to test whether that premise is true in practice. It is deliberately cheap to
build and deliberately narrow, so that a negative answer costs little.

### Target users / personas

Both personas are live in v1, because v1 is published as-is (see Design Decisions). Their full
definitions are minted in *Contracts*.

The **operator** owns the rig, the PC and the device, races iRacing, and is comfortable flashing
firmware and rebuilding a C# plugin. Every decision about feel, thresholds and colour is theirs.

The **adopter** owns a CYD and runs SimHub, and is not assumed willing to edit source or install a
toolchain to get onto their own WiFi. This persona is the sole reason provisioning is a runtime
captive portal rather than compile-time constants, and the reason setup documentation and the
inbound-licence audit are v1 deliverables rather than v2.

### Goals & success criteria

Success is judged **in ordinary use, by the operator**, who records each criterion as met or not
met. How much seat time that takes is deliberately not prescribed: the judgement is the operator's,
and no session count would make it more true (operator decision, 2026-09-21).

The verification method for every criterion below is therefore **manual, by the operator**. That is
stated plainly rather than dressed up as instrumentation — an honestly-declared manual method is a
legitimate check; a fabricated procedure that nobody intends to follow is not.

The criteria themselves are minted as `SUCCESS-*` entries in *Contracts*. In summary, they cover:
gear readability at a glance, peripheral catch of the shift cue, proximity noticed from the bar
rather than the mirrors, absence of distraction, night comfort, diagnosability of a broken link
from the panel alone, and unaided setup by an adopter from the README.

### Capability register

Eight capabilities, all in scope for v1, minted as `CAP-*` entries in *Contracts*. Six are on the
device and two on the PC side. Every one of them traces to at least one success criterion; the two
that cannot be judged from the driver's seat — publication and provisioning — are judged by their
own acceptance checks instead.

Deliberately **not** capabilities: capture and replay (development tooling), and the title-adapter
module structure (an engineering constraint, below).

### Constraints & assumptions

- Hardware is fixed: ESP32-2432S028R ("Cheap Yellow Display"), 320x240 ILI9341 over SPI, mounted
  **landscape, fixed above the wheel** rather than on the wheel.
- Transport is **WiFi/UDP with no middleman application** on the PC. A hard operator constraint,
  and the reason a first-party SimHub plugin is unavoidable — see Architecture's ADR register.
- Addressing is **device-initiated**: the device holds the PC's address; the plugin holds none.
- **Per-title support is modular, as an engineering constraint rather than a capability.** Each
  sim is one source module implementing a common title-adapter interface, registered in one place;
  adding a title touches one new file and one registration line and cannot alter another title's
  behaviour. Ships as a single plugin assembly. [REVISIT] Runtime-loadable adapter assemblies were
  considered and deferred; the interface is designed so that change remains possible.
- **The device is title-agnostic.** The adapter resolves everything sim-specific — including the
  shift-light fallback chain — and emits a normalised frame carrying absolute RPM thresholds.
- **One credential protects both the configuration page and OTA updates.** A single secret, stored
  once; the mechanism is owned by Security & Privacy.
- Build toolchain is the Arduino IDE with OTA updates. Licence is MIT.
- The sim PC and the device share one LAN that permits client-to-client traffic. **Confirmed by the
  operator**, not inferred. It remains worth stating because a guest network or an access point
  with client isolation enabled would break the product silently, with no error that points at the
  cause — an adopter on a different network setup is the case this protects.

### Risks

- **The concept itself may not be worth building.** This is the risk v1 exists to retire, and
  scoping to iRacing is the mitigation: the cheapest complete test of whether a four-element panel
  above the wheel earns its place. The risk is only retired if v1 is judged honestly, including
  the option of stopping.
- **The seat-time criteria are self-reported by the person who built it.** A known weakness of the
  measurement approach chosen, not a reason to change it. The adopter persona provides the only
  outside signal, via the unaided-setup criterion.
- **The title-adapter boundary is being designed against a single example.** An interface with one
  implementation usually encodes that implementation's assumptions. iRacing is the richest source,
  so the likeliest error is an interface that cannot express a *poorer* one — ETS2's total absence
  of proximity data is the obvious test case, and it should be sketched against the interface
  before the interface is called done, even though ETS2 is v2.
- **SimHub's plugin SDK is not a stable published API.** A SimHub update can break the plugin.
  Owned in detail by Integrations.
- **Publishing v1 creates an obligation that outlives the experiment.** If the operator's verdict
  is No-Go, an adopter may still be running it. [GAP] What is owed to adopters in that case — an
  archive notice, a final release, nothing — is undecided, and it is cheaper to decide now than
  after the fact.

## Open Questions

- [GAP] What is owed to adopters if the v2 verdict is No-Go — see Risks.
- [GAP] The iRacing mapping table is authored but **unobserved against a live session**. The
  properties, types and semantics are recorded; what is owed is confirmation, which build step 1
  performs. Owned by Integrations; named here because three capabilities depend on it.
- [REVISIT] **Night brightness.** Partly answered by `CAP-BLANK`, which darkens the panel entirely
  when it is not driving — the idling case, which is most of the time a rig is powered on. What
  remains unanswered is brightness *while driving*: the bands are still full-brightness, there is
  still no auto-dim, and the LDR on GPIO 34 is still unused by decision. `SUCCESS-NIGHT-COMFORT`
  tests that residue.
- [FUTURE-SCOPE] ETS2 was previously scoped to gear only; that decision stands and carries into
  its v2 adapter, which reports shift and proximity as unavailable rather than suppressing them on
  the device.

## Dependencies & Cross-references

This concern owns the root of the traceability web and consumes little. What it references:

| Consumed from | What | Why here |
|---|---|---|
| User Experience | the screen and navigation contracts | Every device-side capability is realised by a screen; this concern names capabilities, not layouts. |
| Interfaces & Contracts | the telemetry frame and registration messages, and the error model | `CAP-PUBLISH` is defined in terms of emitting frames whose schema lives there. |
| Domain & Data | the telemetry frame, display state and device configuration entities, and their invariants | Capability descriptions here use those terms without redefining them. |
| Architecture | the component decomposition and the ADR register | The modular-adapter and title-agnostic-device constraints above are recorded as decisions there. |
| Security & Privacy | credential handling and the shared configuration/OTA secret | Named as a constraint here, owned there. |
| Integrations | the iRacing dependency and its property mapping | Three capabilities are undeliverable without it. |
| Quality & Testing | the seat-time checklist and the adapter conformance suite | The checks that make the criteria below observable. |
| Delivery Process | the v2 Go/No-Go gate | The decision the success criteria feed. |

**No non-functional target is referenced** — see Non-goals. Nothing in this concern points at a
`PERF-*` or equivalent, because none exists by decision.

### Blanking the panel, and what it costs

`CAP-BLANK` is in tension with a position this doc set takes elsewhere, and the tension is
deliberate rather than overlooked. `ENTITY-FRAME` has the plugin publish frames *whenever it is
alive* so that **silence means exactly one thing**, and `SCREEN-LINK` exists so that a panel which
is not driving still says why. A dark panel says nothing at all, and is indistinguishable from a
dead one.

**Decided by the operator 2026-09-23, with this argument:** if the panel fails to light when
telemetry returns, that is a bug which will be noticed immediately — it is not a subtle ambiguity
but an obviously broken product. And a panel that will not light at all is a larger and more visible
problem than an ambiguous dark one. The diagnosability that blanking costs is diagnosability of a
state the operator is not looking at; the diagnosability that matters is preserved, because
`API-STATE` reports whether the backlight is on and is reachable while the glass is dark.

Two conditions are exempt, because both are messages a human is expected to read and act on:
**`SCREEN-UPDATE`**, where a dark panel mid-transfer reads as a crash and invites someone to pull
the power — the one action that can actually brick the device — and **`versionMismatch`**, the single
link condition whose whole purpose is to tell a person which half to update.

`SCREEN-SETUP` needs no exemption: it is shown from the provisioning callback while `autoConnect`
blocks, so the loop that runs the blanking timer is not executing at all. It is exempt by
construction, and that is worth stating because it would otherwise look like an oversight.

## Examples / Worked scenarios

**A race stint.** The device is powered with the rig. It joins WiFi, resolves the configured PC
host, and registers. The operator starts iRacing; SimHub loads; the iRacing adapter begins
resolving frames and the plugin streams them. The panel shows the gear large and centred, the
background neutral at low RPM, stepping the bands green, yellow, amber as revs climb, flashing at the
shift point. Mid-race a car draws alongside on the left and the left bar lights until it clears.
The session ends, telemetry stops, and the panel returns to the idle screen.

**First setup by an adopter.** A stranger flashes the firmware and powers the device. With no
stored configuration it raises its own access point. They join it from a phone, choose their
network, enter the password and the sim PC's address, and save. The device reboots, connects, and
shows the idle screen with its status. They install the plugin into SimHub, start iRacing, and the
panel comes alive. They did not contact the operator at any point — that is the criterion.

**A mistyped PC address.** The adopter enters the wrong address — though the portal's verification
sequence would normally catch it at setup, which is why that sequence exists. If it survives to
runtime, the device joins WiFi and receives nothing, and the panel names the condition but not the
address. Finding the typo means opening the configuration page. Two accepted limitations compound
here: the panel carries no address, and a wrong address looks identical to a switched-off PC,
because the plugin does not acknowledge registration.

**An unsupported title.** The operator loads Assetto Corsa on a v1 device. Only an iRacing adapter
exists, so no adapter matches the running title. The device must say so plainly rather than fall
back to a generic idle screen, because this will happen within days of the first install and a
silent device reads as a broken one.

**Changing the sim PC.** The operator replaces the PC and its address changes. The device still
connects to WiFi, so the portal does not raise. They browse to the device's own address on the
LAN, authenticate with the shared credential, and update the host.

## Design Decisions

Decisions owned by this concern. Cross-cutting technical decisions live in Architecture's ADR
register; decisions about how something looks or is protected live with their concerns.

| Decision | Rationale | Consequence accepted |
|---|---|---|
| v1 is iRacing-only | Only title where all four elements work, so the only one that tests the concept rather than a subset | Two sims the operator plays are unsupported at launch; "unsupported title" becomes an everyday v1 state |
| Success is judged in ordinary use, with no numeric targets and no prescribed procedure | The product's value is perceptual; instrumenting it would measure something other than what matters, and fixing a session count would add ceremony rather than truth | A slow-but-tolerable flash is never caught; Performance stays out of scope; every criterion's verification method is manual and declared as such |
| The v2 Go/No-Go verdict is reserved to the operator | It is a judgement about whether the thing is worth having, not a test result | The verdict is not a testable criterion; it is a named gate with a named decision-maker in Delivery Process |
| v1 is published as-is | Sharing early gets outside signal that self-assessment cannot | The adopter persona is live in v1, making setup docs and the licence audit v1 deliverables |
| Capture & replay is tooling, not a capability | No user experiences it; Quality depends on it internally | No user docs, no format stability commitment |
| The adapter module structure is a constraint, not a capability | No user experiences modularity — they experience "my sim is supported" | Nothing in the register traces to it; the adapter conformance suite in Quality provides the guarantee instead |

## Contracts

### Personas

| ID | Persona | Definition | Notes |
|---|---|---|---|
| `PERSONA-OPERATOR` | The owner-driver | Owns the rig, sim PC and device. Races iRacing. Comfortable flashing firmware and rebuilding a C# plugin. Sole decision-maker on feel, thresholds, colour and scope. | The only persona whose judgement gates v2 |
| `PERSONA-ADOPTER` | A sim racer who finds the project | Owns a CYD and runs SimHub. Not assumed willing to edit source, install a toolchain, or contact the author. | Live in v1. Drives the captive portal, the setup README and the licence audit |

### Capability register

All entries are **in scope for v1**. "Judged by" names the success criterion or acceptance check
that proves the capability.

| ID | Capability | Primary persona | Scope | Judged by |
|---|---|---|---|---|
| `CAP-GEAR` | Render the current gear as one or two auto-sized characters (1-18, N, R) at the centre of the panel | both | in | gear-glance criterion |
| `CAP-SHIFT` | Ramp **two bands, at the top and bottom of the centre region,** unlit→green→yellow→amber between the two shift points and flash them above the upper one, continuing to flash while above it. **Amended 2026-09-22 by operator decision**: this was the whole background, and a full-region repaint at ~12 ms against a ~15 ms panel refresh produced a crawling diagonal tear that was more distracting than the cue was useful. Bands repaint in ~4.3 ms. The centre stays black, which also keeps the gear glyph on one known background — see `U5` for the measurement | both | in | shift-peripheral criterion |
| `CAP-SPOTTER-LEFT` | Light the left edge bar while the spotter reports a car alongside on the left | both | in | spotter-noticed criterion |
| `CAP-SPOTTER-RIGHT` | Light the right edge bar while the spotter reports a car alongside on the right | both | in | spotter-noticed criterion |
| `CAP-LINKSTATE` | Whenever live telemetry is absent, show a distinct screen naming **which** of nine link conditions holds, each with its own icon and plain-language line. Diagnostic detail beyond the condition name lives on the configuration page, not the panel | both | in | link-diagnosable criterion, minus its traced exception |
| `CAP-PROVISION` | Capture WiFi credentials and the PC host through a captive portal when unprovisioned or unable to connect, and persist them across reboots | adopter | in | setup-unaided criterion |
| `CAP-RECONFIG` | Allow the PC host and WiFi settings to be changed while connected, through an authenticated configuration page served on the device's own address | operator | in | acceptance check A7 |
| `CAP-BLANK` | Switch the panel's backlight **off** after a configured period in which the driving screen has not been showing, and switch it back on the instant a live frame is accepted. The period is operator-configurable and the feature is disableable | operator | in | `SUCCESS-DARK-WHEN-IDLE` · `SUCCESS-NIGHT-COMFORT` |
| `CAP-WAKE-RIG` | Wake the sim PC from the panel. While the link state is `unreachable`, a touch lights the backlight and offers the action; a second touch sends a Wake-on-LAN magic packet to the rig's stored hardware address | operator | in | `SUCCESS-WAKE-FROM-PANEL` |
| `CAP-PUBLISH` | Read SimHub properties through the title adapter matching the running sim and stream normalised telemetry frames to each registered device | operator | in | acceptance check A8 |

### Product success criteria

Each is recorded by the operator as met or not met, in ordinary use. Verification method: manual.

| ID | Criterion (observable) | Proves |
|---|---|---|
| `SUCCESS-GEAR-GLANCE` | In ordinary use, the driver reports no instance of needing a second look to read the gear | `CAP-GEAR` |
| `SUCCESS-SHIFT-PERIPHERAL` | In ordinary use, the driver takes shifts on the panel's cue without directing their gaze at it | `CAP-SHIFT` |
| `SUCCESS-SPOTTER-NOTICED` | When racing in close company, the driver reports noticing a car alongside from the edge bar before or at the same time as from the mirrors | `CAP-SPOTTER-LEFT`, `CAP-SPOTTER-RIGHT` |
| `SUCCESS-NO-DISTRACTION` | In ordinary use, the driver reports no instance of the panel pulling their eye at a moment it should not have | all four display capabilities |
| `SUCCESS-NIGHT-COMFORT` | When driving in a darkened room, the driver reports the panel is not uncomfortable to sit beside, including during shift flashes | `CAP-SHIFT` |
| `SUCCESS-DARK-WHEN-IDLE` | With the rig powered on and SimHub running but no sim on track, the panel is dark within the configured period, and lights within one frame interval of telemetry resuming. Measured by leaving the rig idling and then driving | `CAP-BLANK` |
| `SUCCESS-WAKE-FROM-PANEL` | With the rig powered off and the panel showing `unreachable`, two touches start the rig, and the panel reaches the driving screen without the operator going to the PC or to another machine. Measured once against the real rig | `CAP-WAKE-RIG` |
| `SUCCESS-LINK-DIAGNOSABLE` | On each occasion telemetry is absent, the operator determines **which link is broken** from the panel alone, without a laptop or a network tool — **minus one traced exception**: the panel names the condition but not the configured address, so distinguishing a wrong-but-resolvable host from a switched-off PC requires the configuration page. Traced to the User Experience decision that the link screen is icon-plus-line with detail on the configuration page | `CAP-LINKSTATE` |
| `SUCCESS-SETUP-UNAIDED` | An adopter goes from a flashed device to a working panel using only the repository README, without contacting the operator | `CAP-PROVISION` |

## Acceptance criteria

Each maps to an observable check. A1–A6 are judged from the driver's seat; A7–A10 are checks that
are not.

| # | Check | Proves |
|---|---|---|
| A1 | The operator records each success criterion as met or not met from ordinary use | all `SUCCESS-*` except setup-unaided |
| A2 | With the sim running, every gear the car offers — including neutral and reverse — renders at the centre and is legible from the driving position | `CAP-GEAR` |
| A3 | Sweeping RPM from idle to the limiter leaves the bands unlit through the lowest fifth of the window, then steps them green, yellow, amber, then flashes at the upper threshold and continues flashing while held above it.<br><br>**Amended 2026-09-23 to match the shipped behaviour, adjudicated by the operator.** This previously read "a monotonic ramp that begins at the lower shift point", which described the pre-retune design and contradicted the amended `U4` in the same breath — two checks on one capability disagreeing. The cue deliberately starts *above* the lower shift point now: the window is narrow and sits near the top of the rev range, so lighting it from the bottom meant the bands were on almost continuously | `CAP-SHIFT` |
| A4 | With the spotter reporting a car on each side in turn, and on both sides at once, the correct bar or bars light and clear | `CAP-SPOTTER-LEFT`, `CAP-SPOTTER-RIGHT` |
| A5 | Stopping the plugin mid-session causes the panel to leave the driving screen and show the link screen naming the condition; the configured address and packet age are read from the configuration page, not from the panel | `CAP-LINKSTATE` |
| A6 | Disconnecting WiFi mid-session causes the device to show the idle screen and rejoin unaided, with no reboot | `CAP-LINKSTATE` |
| A7 | From a second machine on the LAN, the configuration page is reachable at the device's address, rejects an incorrect credential, and on success changes the PC host such that telemetry resumes from a different PC without reflashing | `CAP-RECONFIG` |
| A8 | With iRacing running, the plugin emits frames whose field values match the SimHub properties named in the Integrations mapping table, at the agreed rate, to every registered device | `CAP-PUBLISH` |
| A9 | Starting a device with no stored configuration raises an access point; completing the portal form results in a device that connects and survives a power cycle with its settings intact | `CAP-PROVISION` |
| A10 | A person other than the operator completes setup from the README alone, and the points at which they hesitate are recorded | `SUCCESS-SETUP-UNAIDED` |

