---
artifact: product-doc
role: concern
concern-id: domain-and-data
behavior: core
trigger: always
in-scope-subaspects: [domain-entities-relationships, identifiers, business-invariants-rules, lifecycle-states, data-classification-tags, persistence-storage-schema, migrations-versioning]
current-rung: contract-grade
status: draft
version: 0.6.0
---

# Domain & Data — cyd-sim-dash

> The normalised frame, the display state derived from it, the configuration that survives reboots, and the rules that must hold over all three.

## Purpose & Scope

Four kinds of data exist: the **normalised telemetry frame** that crosses the wire, the **display
state** derived from it, the **device configuration** that survives reboots, and **capture files**
used as test fixtures. This concern owns their shape, identity, lifecycle and invariants.

Delegated elsewhere: the wire *encoding* of a frame is Interfaces & Contracts, which projects its
messages from the entities here; how configuration is *protected* is Security & Privacy; how state
is *rendered* is User Experience.

**No user entity exists.** The product has no accounts, no sessions and no user records — one
device shows one display to whoever is in the seat. The standard is explicit that this absence
needs no defence.

## Non-goals / Out-of-scope

- **`consistency-transactions`** — *absent*. No multi-entity operation exists. Configuration is a
  single small record written whole. No re-entry note owed.
- **`reference-seed-data`** — *absent*. The product ships no seed data. Per-title fallback
  constants belong to their title adapter on the PC, not to a seed dataset. No re-entry note owed.
- **No historical telemetry store.** Frames are consumed and discarded. Capture files exist only
  as test fixtures on the PC and are not a product data store.
- **No clock synchronisation between PC and device** — *deferred*. The frame stamp orders frames;
  it does not measure latency, because that would require the two clocks to be synchronised and
  nothing here does that. [FUTURE-SCOPE] Re-entry alongside a Performance concern, which would
  need both a synchronisation mechanism and a target to measure against.
- **Erase is available only through the configuration page** — *deferred*. A device whose
  credential is forgotten, or which lands on a network the operator cannot reach, is recoverable
  only by reflashing over USB. Accepted knowingly (operator decision, 2026-09-21).
  [FUTURE-SCOPE] Re-entry is a BOOT-button erase at power-on, which needs no network path.

## Requirements

### Domain entities & relationships

Seven entities, minted in *Contracts* with full field tables. In outline:

A **title adapter** on the PC reads SimHub properties and produces a **frame**, which embeds two
**spotter states** and is transmitted to a device that has announced itself with a
**registration**. The device derives **display state** from the newest acceptable frame plus its
age. The device's persisted settings are its **device configuration**. Each adapter owns a **game
profile** holding its fallback constants. A **capture** is an ordered file of frames.

The relationships that matter: display state is derived, never transmitted; game profile is an
adapter input, never transmitted; and registration flows device-to-PC while frames flow PC-to-device.

### Identifiers

- **Device identity** is derived from hardware, so it needs no storage and no user input. The
  derivation is specified once here because two independent modules compute and compare it: take
  the six-byte MAC of the ESP32 station interface, in the byte order returned by the platform
  (OUI byte first), encode each byte as two lowercase hexadecimal digits, and concatenate with **no
  delimiter** — exactly twelve characters matching `^[0-9a-f]{12}$`.
- **A frame has no independent identifier.** It is ordered within a producer run by its stamp, and
  identified within a capture by its position.
- **A game profile is keyed by the title identity** the adapter declares, which is also the
  dispatch key the plugin core uses to select an adapter. Supplied by `DataCorePlugin.CurrentGame`,
  which returns `"iRacing"` for the v1 title; owned by Integrations.
- **Device configuration is a singleton** on each device.

### Business invariants / rules

Stated as checkable conditions and minted as `INV-*` in *Contracts*. Two points the standard's bar
calls out specifically are handled there: the singleton invariant names a **structural**
enforcement mechanism (the store cannot represent a second record), and the secret-confinement
invariant is explicitly marked **advisory**, because no schema enforces it.

The **shift-threshold fallback chain** is a rule rather than an invariant, and it is stated once
here because both the adapter and its tests implement it:

1. If the adapter obtains both shift-light points as **absolute RPM** — which for iRacing means the
   sim's own `DriverCarSL*` values rather than any computed approximation — and they are **both
   strictly positive**, strictly ordered, and no greater than the redline, use them as the
   ramp-start and flash thresholds.

   *Strictly positive* is load-bearing rather than pedantic: a car with no shift lights configured
   is expected to report zeros, and zeros satisfy "ordered" under a non-strict reading. Accepting
   them would put the ramp start at 0 rpm and light the panel permanently, which is worse than the
   fraction rule and much worse than reporting unavailable.
2. Otherwise, if a positive redline is available, derive them from the game profile's fractions —
   **0.88 of redline for ramp start, 0.97 for flash** by default.
   The redline quantity is **maximum RPM**, which SimHub's iRacing reader sources from the sim's
   own `DriverCarRedLine` — so for iRacing the two are the same number. Resolved by research
   2026-09-21; owned by Integrations.
3. Otherwise, report shift indication **unavailable**. Never invent thresholds.

### Lifecycle & states

**Device configuration:** *unprovisioned* → *provisioned* → *erased*, which returns to
unprovisioned. Erase is reachable only from the configuration page. A record that cannot be read —
corrupt, or written by a schema more than one version old — is discarded and the device returns to
*unprovisioned*, raising the setup portal.

**Device link.** This concern owns the rule that decides which of the nine `linkState` values holds
at any instant. User Experience owns what each one looks like; it does not decide which one is true.

The rule is a **first-match-wins ladder**, evaluated on every render. Ordering it this way is the
substance, not the decoration: link-layer facts outrank frame contents, and freshness outranks what
a stale frame happened to say.

| # | Condition | `linkState` |
|---|---|---|
| 1 | Not associated with WiFi | `joining` |
| 2 | Associated, but the configured host has not resolved | `unresolved` |
| 3 | The most recent datagram was rejected for an unaccepted protocol major, and nothing has been accepted since | `versionMismatch` |
| 4 | No frame has **ever** been accepted this session, and less than the first-frame grace period has passed since registering | `drivingPending` |
| 5 | No frame has ever been accepted this session, and the grace period has passed | `unreachable` |
| 6 | A frame was accepted earlier, but none within the staleness threshold | `stale` |
| 7 | The newest accepted frame is fresh, status `noSim` | `noSim` |
| 8 | The newest accepted frame is fresh, status `unsupportedTitle` | `unsupportedTitle` |
| 9 | The newest accepted frame is fresh, status `adapterFault` | `adapterFault` |
| — | The newest accepted frame is fresh, status `live` | `null` — the driving screen shows |

Two consequences worth stating, because both are questions an implementer would otherwise have to
guess at:

- **A stale fault is `stale`, not the fault.** An `adapterFault` frame that arrived four seconds ago
  yields `stale`, because row 6 precedes row 9. An old fault says nothing about now.
- **`drivingPending` and `unreachable` differ only by elapsed time.** Both mean no frame has ever
  arrived; the grace period is what separates "starting up" from "the PC side is not there".

The first-frame grace period is **5 seconds**. It must comfortably exceed the idle publish interval,
because a device that registers while the PC sits in a menu waits a full idle period (~1 s) for its
first frame; five seconds also absorbs a registration lost to UDP and the next keepalive at 2 s.

**Frame:** *produced* → *transmitted* → *accepted* or *discarded*. UDP offers no delivery
guarantee, so loss is a normal state rather than an error, and a discarded frame is not a failure.

### Data classification tags

| Field | Tag | Handling |
|---|---|---|
| WiFi password | `secret` | Never rendered, logged, transmitted or captured |
| Device credential | `secret` | As above; protects the configuration page and OTA |
| WiFi SSID | identifying | Not secret, but excluded from captures |
| PC host | identifying | Shown on the idle screen by design; excluded from captures |
| Device identity | identifying | Hardware-derived; excluded from captures (operator decision, 2026-09-21) |
| All telemetry fields | none | Gear, RPM, thresholds and spotter states carry no sensitivity |

No personal data of any kind is handled, by either component.

### Persistence / storage schema

Device configuration persists in ESP32 NVS, in a single namespace, as individually typed keys plus
a schema version. Field lengths follow the standards that already bound them: SSID at most 32
octets (802.11), WPA2 passphrase 8 to 63 characters, host name at most 253 characters (DNS).
The device credential's length and composition policy is owned by Security & Privacy and is now
stated there: **non-empty, with no minimum length and no composition rule**.

Game profiles ship compiled into their title adapter on the PC. Tuning shift feel is therefore a
plugin rebuild rather than a reflash — the panel stays mounted. [REVISIT] Whether they should also
be editable from SimHub's plugin settings is worth considering once the feel has been judged.

Captures are **line-delimited JSON** on the PC: one frame per line exactly as it went over the
wire, preceded by its offset from the start of the capture. Chosen so a capture is readable,
greppable and diffable, and so a truncated file from a crashed capture is still usable up to the
last complete line.

### Migrations & versioning

Two independently versioned things exist and must never be conflated: the **wire protocol version**
— a major and a minor, owned as entity fields here and given its compatibility rule by Interfaces &
Contracts — and the **configuration schema version**, owned here outright.

The migration commitment is deliberately bounded: firmware reads and upgrades a configuration
record **exactly one schema version old**; anything older is treated as unreadable and the device
returns to unprovisioned. This keeps settings across the update that actually happens — the next
one — without an unbounded obligation. The cost is that skipping two releases silently loses
settings, which is accepted.

## Open Questions

- ~~The title-identity property.~~ **Resolved** — `DataCorePlugin.CurrentGame`, returning `"iRacing"`.
  Owed by Integrations.
- Credential policy — **resolved**; `SEC-CREDENTIAL-POLICY` sets it as non-empty, with no minimum
  length and no composition rule.
- [REVISIT] Whether game profiles become runtime-editable from SimHub's settings.

## Dependencies & Cross-references

| Consumed from | What | Why |
|---|---|---|
| Product & Requirements | the capability register | Entities exist to serve capabilities; the frame serves gear, shift and both spotter capabilities |
| Interfaces & Contracts | the wire messages | Those messages project from the entities here; encoding, field naming and absence conventions are owned there |
| Integrations | the title-identity property and the per-adapter mapping | Supplies the profile key and the inputs the fallback chain consumes |
| Security & Privacy | credential policy and protection mechanisms | Tags are declared here; protection is owned there |
| Architecture | the title-adapter boundary | The reason no entity here carries per-title behaviour |

No `PERSONA-*` is referenced, because there is no user entity to link one to.

## Examples / Worked scenarios

**A frame through the chain**, with the numbers taken from the capture of 2026-09-22 rather than
invented. iRacing reports 6 353 rpm, shift lights at 6 130 and 6 690, redline 7 500, and a car on
the left. Rung 1 applies: both values are positive, ordered, and below the redline. The adapter
emits a frame stamped 41 250 carrying those two absolute thresholds, gear 4, and left spotter *one
car*. The device compares the stamp to the newest it has seen (41 234), accepts it, records
arrival, and derives a display state: gear glyph `4`, shift phase ramping at roughly 0.4 of the way
between the thresholds, left bar lit, right bar dark.

Those thresholds belong to the car that was driven. A different car yields different ones, resolved
the same way, and neither the wire nor the device knows the difference — which is what keeps the
firmware free of per-car knowledge as well as per-title knowledge.

**A sim with no shift lights.** The adapter obtains a redline of 6 500 but no usable shift-light
values, so the fallback chain derives ramp start at 5 720 and flash at 6 305. The frame is
indistinguishable to the device from one whose thresholds came from the sim — which is the point of
resolving them before the wire.

**A dropped burst.** WiFi stalls for 900 ms. No frames arrive. The device keeps rendering the last
display state, because 900 ms is within the two-second threshold. Frames resume and rendering
continues. Nothing is reported, because this is normal.

**The PC is switched off mid-corner.** Frames stop. For up to two seconds the panel keeps showing
the last gear and, if RPM was above the flash point, keeps flashing. At two seconds it falls to the
idle screen. That flash is stale and potentially misleading — an accepted consequence of the
threshold chosen (see Design Decisions).

**SimHub restarts.** The plugin's stamp counter resets to near zero, far below the newest stamp the
device has seen. Rather than discarding every frame forever, the device recognises a backwards jump
larger than the staleness threshold as a new producer run and resets its newest-seen stamp.

**A firmware update two versions later.** The stored record is two schema versions old. The device
cannot migrate it, discards it, and raises the portal. Settings are lost and re-entered.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| Staleness threshold of two seconds | Brief WiFi stalls are routine; a shorter threshold flickers the panel to idle mid-corner, which the no-distraction criterion is meant to prevent | The panel can assert a stale gear and a stale shift flash for up to two seconds after the PC has gone |
| Frames carry a monotonic stamp, not a sequence number | Orders frames and gives a natural producer-run reset rule | Does not measure latency; that would need clock synchronisation, which does not exist |
| Producer-run reset by backwards-jump detection, rather than a run identifier | One rule, no extra field, deterministic | A backwards jump smaller than the staleness threshold is indistinguishable from reordering — harmless, since both resolve to discarding one frame |
| Gear domain stays 1–18 plus N and R | Keeps the entity and the wire contract stable when a v2 truck adapter arrives | Rendering work in v1 that v1 never exercises |
| Fixed UDP port, compiled in | Preserves the plugin's zero-configuration property, which the device-initiated design exists to provide | A port conflict on the PC has no escape hatch short of a rebuild |
| Device identity derived from the MAC | Unique and stable with no storage and no setup step | A hardware identifier, hence tagged identifying and excluded from captures |
| Migration bounded to one version back | Covers the update that actually happens without an unbounded obligation | Skipping two releases loses settings silently |
| Unreadable config returns to unprovisioned | Always recovers; never misreads an old record as meaning something it does not | Settings are lost without explanation — the portal simply appears |

## Contracts

### Entities

| ID | Entity | Fields (type — meaning) | Notes |
|---|---|---|---|
| `ENTITY-FRAME` | Normalised telemetry frame | `protocolMajor` int · `protocolMinor` int · `status` one of `live` \| `noSim` \| `unsupportedTitle` \| `adapterFault` · `titleId` string or absent — declaring adapter, or the unsupported title's identity · `stamp` int — monotonic ms within a producer run · `gear` gear-value or *unavailable* · `rpm` number ≥ 0 or *unavailable* · `rampStartRpm` number or *unavailable* · `flashRpm` number or *unavailable* · `spotterLeft` `ENTITY-SPOTTERSTATE` · `spotterRight` `ENTITY-SPOTTERSTATE` | Produced by an adapter, consumed by firmware. Carries **no** raw shift-light fractions and **no** redline — those are adapter inputs, resolved before the wire. Published whenever the plugin is alive, including when nothing is driving, so that **silence means exactly one thing**: the PC side is unreachable |
| `ENTITY-SPOTTERSTATE` | Proximity for one side | one of `none` · `one` · `two` · `unavailable` | Domain follows iRacing, the only v1 source. `unavailable` means the title has no proximity source and is **not** the same fact as `none` |
| `ENTITY-DISPLAYSTATE` | What the panel should show | `gearGlyph` 1–2 chars · `shiftPhase` one of `neutral` \| `ramping` \| `flashing` \| `unavailable` · `rampPosition` 0..1 when ramping · `barLeft` bool · `barRight` bool · `linkState` **nullable** — `null` when the driving screen is showing, otherwise one of `drivingPending` \| `noSim` \| `unsupportedTitle` \| `adapterFault` \| `stale` \| `joining` \| `unresolved` \| `versionMismatch` \| `unreachable` | Derived on the device from the newest acceptable frame plus its age. Never transmitted, never persisted |
| `ENTITY-REGISTRATION` | Device announcement and keepalive | `protocolMajor` int · `protocolMinor` int · `deviceId` string — see Identifiers · `firmwareVersion` string | Device to PC, repeated on an interval. The plugin keys its table on `deviceId` and replies to the packet's source address |
| `ENTITY-DEVICECONFIG` | Persisted device settings | `schemaVersion` int · `ssid` ≤32 octets · `wifiPassword` 8–63 chars, `secret` · `pcHost` ≤253 chars · `credential` `secret` | Singleton per device. No port field — the port is fixed |
| `ENTITY-GAMEPROFILE` | Per-adapter fallback constants | `titleId` string · `rampStartFraction` 0..1, default 0.88 · `flashFraction` 0..1, default 0.97 | Lives on the PC inside its adapter. Never transmitted |
| `ENTITY-CAPTURE` | Recorded fixture | ordered lines of `{offsetMs, frame}` | Line-delimited JSON. Contains only frame fields, so it carries nothing tagged `secret` or identifying |

Gear-value domain, referenced above: `R`, `N`, or an integer 1–18.

### Invariants

| ID | Invariant | Enforcement |
|---|---|---|
| `INV-RAMP-ORDER` | When shift indication is available, ramp-start is strictly less than flash | Adapter, validated on the device; a violating frame is rejected whole |
| `INV-RPM-NONNEG` | RPM is a finite number, not negative | Device validation on receipt |
| `INV-GEAR-DOMAIN` | Gear is `R`, `N`, or an integer 1–18 | Device validation on receipt |
| `INV-STAMP-ORDER` | Within a producer run, stamps strictly increase; the device accepts a frame only if its stamp exceeds the newest seen, except that a backwards jump larger than the staleness threshold is a new producer run and resets the newest-seen value | **By construction** — the plugin increments a monotonic counter per frame; the device holds one comparison value |
| `INV-FRESH-RENDER` | The driving screen renders only from a frame whose local arrival is within **two seconds**; past that the device shows link state instead | Device timer against its own clock, never the frame stamp |
| `INV-UNAVAILABLE-DISTINCT` | An element a title cannot source is emitted as `unavailable` and never as a value; in particular a missing proximity source is never emitted as `none` | Adapter conformance suite; violating frames are rejected on the device |
| `INV-VERSION-WHOLE` | A frame whose protocol **major** version the firmware does not accept is discarded entire, never partially decoded. A differing minor is tolerated, and unknown fields are ignored | Device validation before field access |
| `INV-STATUS-CONSISTENT` | A frame whose status is not `live` carries every telemetry element as *unavailable*, and never a value. A frame whose status is `live` carries no element as unavailable unless the title genuinely lacks that source. **`titleId` is not a telemetry element** and is exempt: an `unsupportedTitle` frame must carry the title's identity, since naming it is the entire point of that status | Adapter conformance suite; validated on the device, which rejects a violating frame whole |
| `INV-CONFIG-SINGLETON` | Exactly one configuration record exists per device | **Structural** — one fixed NVS namespace and key set, written as a single operation; the store cannot represent a second record |
| `INV-CONFIG-MIGRATION` | A record exactly one schema version old is migrated in place; any older or unreadable record is discarded and the device returns to unprovisioned | Device, on read at boot |
| `INV-CAPTURE-CLEAN` | A capture contains no field tagged `secret` or identifying | **By construction** — captures serialise `ENTITY-FRAME` only, and no such field appears in it |
| `INV-SECRET-CONFINEMENT` | Secrets never appear on any screen, in serial output, in logs, in a capture, or in any wire message | **Advisory** — no schema enforces this; it is held by review and by the tests named in Quality & Testing, and can be violated by a careless code change |

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| D1 | Frames with an inverted threshold pair, a negative RPM, or a gear outside the domain are each rejected and leave the previous display state untouched | `INV-RAMP-ORDER`, `INV-RPM-NONNEG`, `INV-GEAR-DOMAIN` |
| D2 | Replaying a capture with two frames transposed yields the same display states as replaying it in order | `INV-STAMP-ORDER` |
| D3 | Restarting the producer mid-replay, so stamps reset to zero, resumes rendering rather than discarding every subsequent frame | `INV-STAMP-ORDER`, producer-run reset |
| D4 | Withholding frames for 1.5 s leaves the driving screen intact; withholding for 2.5 s moves it to link state | `INV-FRESH-RENDER` |
| D5 | An adapter that reports a value for an element it has no source for is rejected by the conformance suite | `INV-UNAVAILABLE-DISTINCT` |
| D6 | A frame bearing an unrecognised protocol version changes nothing on the panel and is counted as rejected | `INV-VERSION-WHOLE` |
| D13 | Frames with status `noSim`, `unsupportedTitle` and `adapterFault` each carry only unavailable telemetry, and each yields a distinguishable link state; a frame carrying both a non-`live` status and a gear value is rejected | `INV-STATUS-CONSISTENT` |
| D7 | Provisioning, power-cycling, and reading back the record yields identical values; writing twice leaves one record | `INV-CONFIG-SINGLETON` |
| D8 | A record written at schema version *n−1* is migrated and settings survive; one at *n−2* is discarded and the portal raises | `INV-CONFIG-MIGRATION` |
| D9 | A capture taken from a provisioned device, searched for the SSID, host, credential and device identity, contains none of them | `INV-CAPTURE-CLEAN` |
| D10 | Serial output and every screen are inspected during a full provisioning and driving cycle; no secret appears | `INV-SECRET-CONFINEMENT`, advisory |
| D11 | Two device identities derived from different boards differ, and one derived twice from the same board is identical and matches the stated format | Identifier derivation |
| D12 | With shift-light values absent and a redline of 6 500, the adapter emits ramp start 5 720 and flash 6 305; with the redline also absent, it emits shift unavailable | Fallback chain |

