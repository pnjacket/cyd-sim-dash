---
artifact: product-doc
role: concern
concern-id: architecture
behavior: core
trigger: always
in-scope-subaspects: [component-decomposition-responsibilities, boundaries-isolation-model, component-interactions-data-flow, cross-cutting-patterns, technology-choices, adr-register, logical-deployment-topology]
current-rung: contract-grade
status: published
version: 0.7.0
---

# Architecture — cyd-sim-dash

> Two nodes, nine components, one device-initiated UDP stream — and the constraint that forced the shape.

## Purpose & Scope

How the product is decomposed, how the pieces talk, and why. The central architectural fact is
that a hard operator constraint — **WiFi with no middleman application on the PC** — eliminated
every off-the-shelf SimHub integration path and made a first-party SimHub plugin unavoidable.
Everything else follows from that, and from the later decision that per-title support is modular.

Delegated elsewhere: the wire schema to Interfaces & Contracts; entity shapes and invariants to
Domain & Data; screen appearance to User Experience; physical deployment to Operations, which is
out of scope because there is no infrastructure to operate.

## Non-goals / Out-of-scope

- **`scalability-resilience-patterns`** — *absent*. One PC, one device, one viewer. There is
  nothing to scale, and resilience here reduces to reconnection, covered by a cross-cutting
  pattern. No re-entry note owed.
- **Multiple devices from one plugin** — *deferred*. [FUTURE-SCOPE] The device-initiated
  registration design permits it — the publisher already holds a table keyed on device identity
  rather than a single address — so this is a deliberate non-goal, not a structural barrier.
- **No runtime-loadable adapters** — *deferred*. Adapters are source modules in one assembly.
  [FUTURE-SCOPE] Re-entry needs a versioned public interface, which nothing yet requires.
- **No clock synchronisation between nodes.** Owned as a non-goal by Domain & Data; named here
  because it bounds what the component model can offer.

## Requirements

### Component decomposition & responsibilities

Nine components, four on the PC and five on the device, minted in *Contracts* with their
responsibilities, owned interfaces and dependencies.

The decomposition exists to protect two properties. First, **no sim-specific knowledge outside a
title adapter** — the plugin core dispatches, the adapter knows the sim, and the device knows
neither. Second, **no rendering work on the path that receives frames**, which is why the device
splits network and rendering across the ESP32's two cores.

### Boundaries & isolation model

Three boundaries, each with a different trust posture:

- **Inside SimHub's process.** The plugin runs as a guest in an application the operator's whole
  rig depends on. Nothing may escape into the host: the fault boundary is absolute, and a
  misbehaving adapter degrades this product rather than SimHub.
- **The LAN.** Everything crossing it is unauthenticated plaintext UDP, plus two authenticated
  HTTP surfaces. Any device on the network can send a malformed datagram, so the receive path
  treats input as untrusted. Security & Privacy owns the trust argument.
- **The device.** Frames are accepted only from the configured host and only at a recognised
  protocol version.

### Component interactions / data flow

The inversion is deliberate and is what makes the plugin configuration-free:

1. The device boots, joins WiFi, and resolves the configured host.
2. The device sends a registration to the fixed port, repeating it on an interval as a keepalive.
3. The publisher records the **source address** of that registration against its device identity,
   and begins unicasting frames back.
4. Each update, the plugin core selects the adapter matching the running title, obtains a frame,
   and hands it to the publisher.
5. The device validates, decodes, orders by stamp, and derives display state; the renderer paints
   only what changed.
6. If frames stop for longer than the staleness threshold, the device shows link state and keeps
   registering.

Consequences worth stating: the device's own address may change freely under DHCP; the plugin
needs no configuration; no broadcast traffic is generated; and the publisher knows a device has
gone because registrations stop.

**The plugin publishes even when there is nothing to drive.** A frame carries a status saying
whether telemetry is live, no sim is running, the running title has no adapter, or an adapter has
faulted. Silence therefore means one thing only — the PC side is not reachable — which is what
makes the link diagnosable from the panel alone.

### Cross-cutting patterns

Five patterns, minted in *Contracts*. The error-handling pattern carries the weight: the standard
requires it to be **total** — covering errors from every source, including the runtime and the
libraries, not only those this product raises — and for a single-surface client product it must
**never be console-only**. Serial output behind a mounted panel is console-only in practice, so
every error class is routed either to a screen or to a counter on the configuration page.

### Technology choices

| Choice | Selection | Rationale |
|---|---|---|
| PC host | SimHub plugin, C#, .NET Framework 4.8 | Matches SimHub's own runtime |
| Wire transport | UDP over WiFi, fixed port | Operator constraint; fixed port keeps the plugin configuration-free |
| Wire encoding | JSON text, versioned | Inspectable with any UDP listener |
| Device platform | ESP32-2432S028R, Arduino framework | Hardware already owned |
| Device toolchain | Arduino IDE, OTA updates | Lowest barrier for the adopter persona |
| Graphics | TFT_eSPI | De-facto CYD library; direct and light for four hand-drawn elements |
| JSON on device | ArduinoJson | Parsing untrusted input well is fiddly; this is the hardened, well-trodden option |
| Provisioning | WiFiManager | Standard captive portal; accepted as a third external dependency |
| Replay tool | Standalone Python script | Must run with SimHub shut, which is its entire purpose |

**Dependency posture.** Three external device libraries — TFT_eSPI, ArduinoJson, WiFiManager —
each an unpinned install step for an adopter, since the Arduino IDE pins nothing. The mitigation is
Integrations' obligation to record exact tested versions. TFT_eSPI carries a specific, well-known
hazard: its pin configuration lives in the library's own header rather than in the sketch, and
getting it wrong produces a blank screen with no error. That single fact is the most likely cause
of an adopter failing to get a working panel, and the setup documentation must treat it as such.

### Logical deployment topology

Two nodes on one LAN: the sim PC (SimHub with the plugin, and during development the replay tool)
and the device. No servers, no cloud, no internet egress.

## Open Questions

- [GAP] The iRacing property mapping is authored but **unobserved**; Integrations sits at
  `specified` until build step 1 confirms each row. The adapter implements it either way — what is
  owed is confirmation, not content.
- OTA exposure — **resolved**; the window rule is owned by `SEC-OTA-WINDOW`.
- [REVISIT] Whether the frame-writing code is factored so the Python replay tool and the C#
  publisher cannot drift. They are in different languages, so sharing code is not available; a
  shared fixture set is the realistic substitute and belongs to Quality & Testing.

## Dependencies & Cross-references

| Consumed from | What | Why |
|---|---|---|
| Product & Requirements | the capability register | Every component exists to realise capabilities; the mapping is in the component table |
| Domain & Data | the entity and invariant registers | Components own and validate these; the receive path enforces them |
| Interfaces & Contracts | the wire messages and error catalogue | The data-flow above is defined in terms of them |
| Security & Privacy | trust posture for the LAN and the two HTTP surfaces | Boundaries are named here; their defence is owned there |
| Integrations | the SimHub SDK and the device library set | Technology choices above depend on them |

## Examples / Worked scenarios

**A frame, end to end.** SimHub's update fires. The plugin core looks up the running title, finds
the iRacing adapter, and calls it. The adapter reads properties, resolves thresholds, and returns a
frame with status *live*. The publisher stamps and serialises it and sends one datagram per
registered device. On the device, the network task validates version and fields, compares the
stamp, stores the frame in the back buffer and swaps. The render task wakes, derives display state,
finds only the gear changed, and repaints the glyph region alone.

**An adapter throws.** A property the iRacing adapter expects is missing after a SimHub update and
it raises. The fault boundary catches it, logs once for that cause rather than sixty times a
second, and the publisher continues sending frames with status `adapterFault`. The panel says so.
SimHub is unaffected.

**Assetto Corsa is loaded on a v1 device.** No adapter matches. The core publishes frames with
status `unsupportedTitle` naming the title. The panel says the title is unsupported rather than
appearing broken.

**A malformed datagram arrives from elsewhere on the LAN.** The network task rejects it at the
first validation that fails, increments a counter, and continues. Nothing reaches the state engine,
nothing is rendered, and the count is visible on the configuration page.

**WiFi drops mid-session.** The network task detects the loss and begins rejoining with backoff,
without blocking. The render task keeps running and, once the staleness threshold passes, paints
the link-state screen. Rejoining restores frames with no reboot and no user action.

**SimHub restarts.** Stamps reset to near zero. The network task recognises a backwards jump
larger than the staleness threshold as a new producer run, resets its newest-seen stamp, and
resumes — rather than discarding every subsequent frame forever.

## Design Decisions

Cross-cutting decisions are minted as `ADR-*` in *Contracts*. Two decisions frequently associated
with architecture are owned elsewhere and are referenced rather than duplicated: the progressive
colour ramp belongs to User Experience, and the landscape, fixed-above-the-wheel mounting is a
constraint in Product & Requirements.

## Contracts

### Components

| ID | Component | Responsibility | Owned interfaces | Depends on |
|---|---|---|---|---|
| `COMPONENT-PLUGIN-CORE` | SimHub data plugin | Receives SimHub updates, selects the adapter matching the running title, obtains a frame, hands it to the publisher. Holds the fault boundary. Contains **no** sim-specific code | The title-adapter interface and its registry | SimHub SDK; the adapters |
| `COMPONENT-ADAPTER-IRACING` | iRacing title adapter | Resolves iRacing's SimHub properties into a normalised frame, including the shift-threshold fallback chain, and reports any element it cannot source as unavailable | Implements the title-adapter interface | SimHub properties; its game profile |
| `COMPONENT-PUBLISHER` | Frame publisher | Owns the UDP socket. Receives registrations, maintains the device table keyed on device identity, serialises and unicasts frames, and optionally writes captures | The UDP surface on the fixed port | `COMPONENT-PLUGIN-CORE` |
| `COMPONENT-REPLAY` | Replay tool | Standalone Python script that reads a capture and emits frames, so firmware work needs neither SimHub nor a sim | Command line | A capture file |
| `COMPONENT-NET` | Device network layer | Joins WiFi, resolves the host, registers on an interval, receives datagrams, validates version and fields, orders by stamp, publishes into the shared buffer. Runs on the WiFi core | The registration keepalive | WiFi stack; ArduinoJson |
| `COMPONENT-STATE` | Display-state engine | Derives display state from the newest accepted frame plus its age. Pure logic, no drawing, **no per-title branch** | — | `COMPONENT-NET` output |
| `COMPONENT-RENDER` | Renderer | Paints display state to the panel using dirty regions; owns layout geometry. Runs on the application core | — | TFT_eSPI; `COMPONENT-STATE` |
| `COMPONENT-WEB` | Portal, configuration page and state endpoint | Serves the provisioning portal when unprovisioned or unable to connect, the authenticated configuration page while connected, and the read-only `API-STATE` endpoint in every build | Three HTTP surfaces | WiFiManager; `COMPONENT-CONFIG`; `COMPONENT-STATE` |
| `COMPONENT-CONFIG` | Configuration store | Reads and writes the persisted record, applies one-version migration, and erases on request | — | NVS |

Capability realisation: gear, shift and both spotter capabilities are realised jointly by
`COMPONENT-NET`, `COMPONENT-STATE` and `COMPONENT-RENDER`; link state by `COMPONENT-RENDER`;
link state jointly by `COMPONENT-NET` (the link condition and packet age), `COMPONENT-CONFIG` (the
configured host, read on the configuration page) and `COMPONENT-RENDER`; provisioning and
reconfiguration by `COMPONENT-WEB` and `COMPONENT-CONFIG`; publication by
`COMPONENT-PLUGIN-CORE`, `COMPONENT-ADAPTER-IRACING` and `COMPONENT-PUBLISHER`.

### Cross-cutting patterns

| ID | Pattern | Specification |
|---|---|---|
| `PATTERN-ERROR` | Total error handling | Every error, from any source — this product's own checks, the JSON parser, the WiFi and HTTP stacks, NVS, and the SimHub SDK — is caught and classified as **hard** or **soft**. Hard faults stop the product working and each gets a distinct on-screen presentation: no WiFi, host unresolvable, protocol-version mismatch, unsupported title, adapter fault, unreadable configuration, and an unknown HTTP path. One further hard fault, **publish-failed**, is **PC-side**: its surface is SimHub's log because the failure is the inability to reach the device at all, so no device surface can exist for it. Soft faults are individually survivable and become named counters on the configuration page: malformed datagram, field out of range, and frame rejected as out of order. A fourth records frames rejected for an unaccepted protocol major — that condition is **hard** and raises its own screen, so the counter accompanies the screen rather than replacing it. All four count since boot and are not persisted. **No error class terminates in serial output alone**, because serial behind a mounted panel is not a human-visible surface |
| `PATTERN-FAULT-BOUNDARY` | Host isolation | No exception escapes the plugin into SimHub. Every entry point from the SDK is wrapped. A fault is logged **once per distinct cause**, never once per update, and is reported onward as a frame status so the device can show it |
| `PATTERN-STATE-HANDOFF` | Cross-core buffer | The network task writes into a back buffer and swaps it under a short critical section; the render task reads the front buffer. The renderer never blocks the receive path and no partially-written frame is ever read |
| `PATTERN-RECONNECT` | Non-blocking recovery | Loss of WiFi or failure to resolve the host triggers retry with backoff, always off the render path. The device never requires a reboot to recover and never stops painting while retrying |
| `PATTERN-LOGGING` | Where messages go | Device writes to serial for anyone with a cable; the plugin writes to SimHub's own log, which users already know how to find; neither is treated as sufficient on its own — see `PATTERN-ERROR` |

### ADR register

| ID | Decision | Consequence accepted |
|---|---|---|
| `ADR-FIRST-PARTY-PLUGIN` | Build a first-party SimHub plugin rather than use any stock integration | Stock serial is serial-only, ESP-SimHub's WiFi needs a COM bridge, and SimHub's UDP forward carries only raw game packets — all excluded by the no-middleman constraint. Cost: C# is a second language and codebase |
| `ADR-DEVICE-INITIATED` | The device holds the PC's address; the publisher learns the device's from the packet source | Rejected: plugin-side fixed IP (breaks on DHCP), subnet broadcast (floods the LAN at frame rate), mDNS (unreliable on Windows). Cost: the device needs the PC's address configured |
| `ADR-JSON-WIRE` | JSON on the wire rather than packed binary | Buys inspectability and extension without lockstep updates. Cost: bandwidth and parse time, both affordable here |
| `ADR-ADAPTER-MODULES` | Per-title support as source modules behind a common interface, one assembly | Rejected: runtime-loaded DLLs (needs a public ABI before anything needs one), one SimHub plugin per title (relies on undocumented inter-plugin discovery). Cost: adding a title requires a rebuild |
| `ADR-NORMALISED-FRAME` | The frame carries resolved absolute RPM thresholds, not raw shift-light values | The device holds no per-title branch and no game profile; tuning is a plugin rebuild, not a reflash; the schema stops growing a field per sim. Cost: anything needing a raw value later requires a frame change |
| `ADR-STATUS-FRAME` | Publish frames even when there is no telemetry, carrying a status | Makes silence mean exactly one thing, so the link is diagnosable from the panel alone. Cost: a status field, and three more device states to specify |
| `ADR-ARDUINO-OTA` | Arduino IDE with OTA updates rather than PlatformIO | Lowest barrier for the adopter persona. Cost: no version pinning, so exact library versions must be documented by hand |
| `ADR-OTA-EXCLUSIVE` | While an update is transferring, the device loop services the update and nothing else — telemetry, rendering and the HTTP surface all pause until it finishes or fails | Rejected: servicing everything concurrently, which is what shipped until 2026-09-23 and which **failed on the rig**. An upload is a TCP stream needing prompt attention; at ~60 Hz the receive path, renderer and web server together starved it and the transfer aborted mid-flight. Cost: telemetry is dropped for those seconds, which is free — the device is about to reboot into new firmware, and a panel mid-update is not one anybody is driving past. A failed update clears the flag, so a botched transfer cannot leave the panel mute |
| `ADR-TFT-ESPI` | TFT_eSPI for graphics | Direct and light for four hand-drawn elements; LVGL is heavier than this UI justifies. Cost: pin configuration lives in the library's header, the best-known cause of a blank CYD screen |
| `ADR-TWO-TASKS` | Split network and rendering across the ESP32's two cores | A full-screen repaint is roughly 150 KB over SPI and blocks its thread for tens of milliseconds; the second core would otherwise sit idle. Cost: a shared-state handoff that must be correct — see `PATTERN-STATE-HANDOFF` |
| `ADR-DIRTY-REGIONS` | Repaint only what changed | Most frames repaint nothing; the flash costs full fills at flash rate rather than frame rate. Cost: the renderer tracks what it has drawn |
| `ADR-STD-LIBS` | Use ArduinoJson and WiFiManager rather than hand-rolling | Hardened parsing on the one path an untrusted LAN device can reach, and a standard portal. Cost: two more unpinned dependencies for an adopter |
| `ADR-REPLAY-STANDALONE` | The replay tool is a standalone Python script | Must run with SimHub shut, which is its purpose. Cost: a second language on the PC side, and no shared serialiser with the C# publisher |

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| R1 | Searching the firmware for the title identity finds it only where it is displayed — never in a conditional governing gear, shift or bar behaviour | No per-title logic on the device |
| R2 | Searching `COMPONENT-PLUGIN-CORE` for any sim name or sim-specific property finds nothing; all such references live in an adapter | Adapter boundary holds |
| R3 | Each **device-side** error class named in `PATTERN-ERROR` is provoked in turn, and each produces either its screen or its counter; none is observable only on serial. The one PC-side class, publish-failed, is asserted against SimHub's log instead | `PATTERN-ERROR` totality |
| R4 | An adapter that throws on every update leaves SimHub running, produces one log entry rather than a stream, and results in a panel showing the adapter fault | `PATTERN-FAULT-BOUNDARY` |
| R5 | Under sustained frames at full rate with the renderer forced into repeated full-screen fills, no frame is missed on the receive path | `PATTERN-STATE-HANDOFF`, `ADR-TWO-TASKS` |
| R6 | Disabling the access point mid-session and restoring it later resumes frames with no reboot and no user action, and the panel keeps painting throughout | `PATTERN-RECONNECT` |
| R7 | Changing only the gear in successive frames results in repaints confined to the glyph region | `ADR-DIRTY-REGIONS` |
| R8 | With no sim running, with an unsupported title, and with a faulted adapter, the panel shows three distinguishable states, and all three differ from the state when the plugin is not running at all | `ADR-STATUS-FRAME` |
| R9 | The device's external library list contains exactly TFT_eSPI, ArduinoJson and WiFiManager, each with its tested version recorded | `ADR-STD-LIBS` |
| R10 | The replay tool drives a device to a correct display with SimHub not running | `ADR-REPLAY-STANDALONE`, `COMPONENT-REPLAY` |

