---
artifact: product-doc
role: concern
concern-id: interfaces-and-contracts
behavior: core
trigger: always
in-scope-subaspects: [http-rpc-api-surface, event-surface, ui-entrypoints, error-model-catalog, versioning-compatibility]
current-rung: contract-grade
status: published
version: 0.10.0
---

# Interfaces & Contracts — cyd-sim-dash

> Two UDP messages, two HTML surfaces, one total error catalogue, and the version rule that keeps two independently-updated halves honest.

## Purpose & Scope

This is the hard boundary of the product: two components, in different languages, on different
machines, agreeing only on what crosses the wire. Two messages travel it — registration and
telemetry frames — and this concern owns both, their concrete field projections, their version
compatibility rule, and what each side does when the other sends something it cannot use.

It also owns the two HTML data-entry surfaces, because a form that takes a WPA2 passphrase and a
host address is a contract even though a human fills it in.

## Non-goals / Out-of-scope

- **`cli-surface`** — *deferred*. The replay tool genuinely has one: it is a standalone script with
  arguments, owned as a component by Architecture. It is calibrated out because it is development
  tooling rather than a product surface, not because no command line exists.
  [FUTURE-SCOPE] Re-entry if the tool is ever shipped to adopters.
- **`library-sdk-surface`** — *absent*. Nothing is consumed as a library. No re-entry note owed.
- **`pagination-filtering-rate-limit-conventions`** — *absent*. A fixed-shape datagram stream has
  no collections to page and no caller to rate-limit. No re-entry note owed.
- **Input bindings and DOM hooks** — *deferred*. The touchscreen is unused in v1, so no input
  binding layer exists. [FUTURE-SCOPE] Returns if touch is used.
- **Registration is unauthenticated** — *deferred*, and stated as a negative assertion rather than
  left as an omission. See *Event surface*. [FUTURE-SCOPE] Re-entry is a shared token or a device
  allowlist, both of which cost the plugin its zero-configuration property.

## Requirements

### Event surface

Two messages, both UDP on the fixed port, both JSON text with **full readable camelCase field
names** — chosen so a datagram dumped from a packet sniffer is self-describing, which is the reason
JSON was picked over a packed struct in the first place.

**The convention for "unavailable" is a single rule, applied to every telemetry element:** the
field is **always present**, and its value is **`null`** when the running title cannot supply it.
Absence of a key never means unavailable; a key is missing only in a malformed message. This is the
one convention that stops a permanently-dark edge bar from looking identical to a working one, and
it is why the *unavailable* member of `ENTITY-SPOTTERSTATE` projects to `null` rather than to a
string.

**Cadence is adaptive by status:** every SimHub update while status is live, and approximately once
per second otherwise. Sixty identical "no sim running" frames per second have no value, and the
reduced rate keeps a packet capture readable when you are trying to debug something.

**Subscription authorization: none.** Any host on the LAN may register and will be sent frames.
Recorded as a deliberate negative assertion with its argument: the telemetry carries gear, RPM and
proximity, none of which has confidentiality value, and the alternatives — a shared token or a
device allowlist — both require configuring the plugin, destroying the zero-configuration property
that the device-initiated design exists to provide. The publisher replies only to the source
address of a registration it actually received, so it cannot be induced to send traffic to a third
party.

### HTTP API surface

**Scope change, 2026-09-21.** This key was previously out of scope as *absent*, on the grounds that
the portal and configuration page serve HTML to a human and nothing programmatic consumes them.
Quality & Testing's decision to verify panel behaviour by asserting on the device's computed display
state makes that false: there is now a machine-consumed endpoint, so the key returns to scope
rather than the endpoint being quietly treated as a non-surface.

One element, `API-STATE`, specified in *Contracts*. It is read-only, unauthenticated, and present
in **every build including release** — deliberately, because an endpoint compiled only into test
builds would mean the end-to-end tests prove a binary that is never shipped. Leaving it open is
consistent with the UDP stream, which is unauthenticated on exactly the same argument: display
state is derived from telemetry that has no confidentiality value.

The OTA path remains outside this concern; it is a protocol the Arduino framework provides rather
than a surface this product designs.

### UI entrypoints

The navigation contract — which screens exist and how the device moves between them — is **owned by
User Experience** and referenced, not duplicated. What this concern owns is the two data-entry
surfaces, specified in *Contracts*: the provisioning portal form and the configuration page form.

Both share their field definitions, validation and empty-value semantics. Empty is always a
validation failure, never a meaningful value: there is no "unset means default" case in either
form, with one labelled exception on the configuration page.

**The portal verifies before it saves.** On submit, the device attempts the whole chain — join
WiFi, resolve the host, register, and wait for a frame — and reports either success or the exact
step that failed, before anything is persisted. This is the single strongest measure available for
the unaided-setup criterion, and it is only possible because the plugin now publishes frames
continuously: without that, "waiting for a frame" would have been indistinguishable from an idle
sim.

### Error model / catalog

The catalogue in *Contracts* is **total**: every error reaches it, including those raised by the
JSON parser, the WiFi and HTTP stacks, NVS and the SimHub SDK, not only those this product raises
itself. A raw library error surfacing to a user is a contract violation.

**One row is classified apart from the rest, and it is not a scope exception.** `ERR-PUBLISH-FAILED`
is a **PC-side hard fault**: its surface is SimHub's own log, because the failure *is* the inability
to send, so no device-observable surface can exist for it — reaching a device is exactly what has
failed. It is a **known limitation with a named cause**, not Part 9's claim-minus-traced-exception
construction, which traces to a scope decision in another concern. Nothing was scoped out here;
the surface is impossible rather than declined. The device-side rule — every hard fault gets a
screen, every soft fault a counter — holds for all fourteen device-side rows without exception.

The content-negotiation half of the standard's rule degenerates here, because there is no
programmatic caller — one human-facing surface per side. The operative rule is therefore **total
and never console-only**: every row terminates at a screen, a form message, or a counter on the
configuration page. Serial output behind a mounted panel is not a human-visible surface, and no row
may rely on it.

Each row carries how its condition is **forced under test**, and the forcing named is usable at the
tier that proves it. Most rows are forced by the replay tool, which can emit any frame including
invalid ones — a real component rather than a hypothetical harness.

### Versioning & compatibility

A two-part version, `protocolMajor` and `protocolMinor`, on both messages:

- **Same major: compatible.** The device ignores fields it does not recognise, so adding a field is
  a minor bump that older firmware survives unchanged.
- **Different major: refused**, with a distinct on-screen presentation naming both versions.
  Removing a field, or changing what one means, is a major bump.
- The device never partially decodes a message whose major it does not accept.
- **The state endpoint is versioned by the firmware, not by the wire protocol** — `API-STATE` is a read-only
  diagnostic surface whose only consumers are a human browser and this project's own tests, both of
  which ship with the firmware that serves it. Fields may be added within a firmware version series
  and consumers must ignore unknown ones; no cross-version compatibility is promised, because no
  independently-updated consumer exists.

This matters more here than in most products: the plugin and firmware update independently, and OTA
makes it easy to update one and forget the other. Version mismatch is an everyday state rather than
an edge case, which is why it gets a screen instead of silence — and why registration carries the
device's firmware version, so the mismatch is also diagnosable from the PC side at the moment the
panel is least able to help.

## Open Questions

- OTA exposure — **resolved**; `SEC-OTA-WINDOW` confines listening to a window opened from the
  configuration page. This concern still does not specify the path itself, because the framework
  provides it.

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Domain & Data | the frame, registration, spotter-state and device-configuration entities project into the messages and forms below; the identifier format and every validation bound are owned there and referenced, never restated with different numbers |
| Architecture | each element names its owning component; `PATTERN-ERROR` classifies each catalogue row as hard or soft |
| Product & Requirements | each element names the capability it serves |
| User Experience | the screens on which hard faults appear |
| Security & Privacy | the credential gating the configuration page, and the OTA surface |

## Examples / Worked scenarios

**A live frame on the wire.**

```json
{"protocolMajor":1,"protocolMinor":0,"status":"live","titleId":"iracing","stamp":41250,
 "gear":"4","rpm":7200,"rampStartRpm":7600,"flashRpm":8000,
 "spotterLeft":"one","spotterRight":"none"}
```

**No sim running.** Status `noSim`, every telemetry element `null`, roughly one per second:

```json
{"protocolMajor":1,"protocolMinor":0,"status":"noSim","titleId":null,"stamp":52310,
 "gear":null,"rpm":null,"rampStartRpm":null,"flashRpm":null,
 "spotterLeft":null,"spotterRight":null}
```

The difference between `"none"` in the first example and `null` in the second is the entire point
of the convention: one says *the track is clear*, the other says *nothing can tell you*.

**A minor version the device has never seen.** The plugin is at `1.3` and sends a field the device
does not know. Same major, so the frame is accepted and the field ignored. Nothing is shown, because
nothing is wrong.

**A major bump.** The plugin is at `2.0`. The device refuses every frame whole, shows the
version-mismatch screen naming `1.x` and `2.0`, and keeps registering — so the plugin can still
display the device's firmware version in SimHub while the panel itself refuses to drive.

**A mistyped host, caught at the portal.** The adopter submits. WiFi joins, the host resolves, the
device registers — and no frame arrives within the wait. The portal reports that it reached the
network but heard nothing from the PC, suggests checking the address and that SimHub is running,
and does not save. They correct the address and it succeeds.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| `null` means unavailable; keys are always present | One rule across every element; absence then unambiguously signals a malformed message | Slightly larger frames, since nothing is ever omitted |
| Full readable camelCase names | Preserves the inspectability JSON was chosen for | A few hundred bytes per frame rather than a hundred — irrelevant on a LAN |
| Adaptive cadence by status | No value in sixty identical idle frames a second; keeps captures readable | Two rates to specify and test rather than one |
| Major/minor with additive tolerance | Adding a field must not brick a mounted panel; re-meaning one must not pass silently | A discipline to maintain: any field whose meaning changes owes a major bump |
| Registration unauthenticated | Preserves the plugin's zero-configuration property; the data has no confidentiality value | Anyone on the LAN can receive your telemetry |
| Portal verifies the full chain before saving | Catches wrong passphrase, wrong address and SimHub-not-running at the only moment the person can fix them | A longer submit step, and portal logic that must handle each failure itself |
| Registration carries firmware version | Makes version mismatch diagnosable from the PC when the panel cannot help | One more field, and a value that must be kept accurate at build time |

## Contracts

### Event elements

| ID | Element | Direction · owner · serves | Wire projection | Pre/post · side-effects |
|---|---|---|---|---|
| `EVT-REGISTRATION` | Device announcement and keepalive | Device → PC, every **2 s** · `COMPONENT-NET` · serves `CAP-PUBLISH` | `protocolMajor` int · `protocolMinor` int · `deviceId` string, validated against the twelve-character lowercase-hex format owned by Domain & Data · `firmwareVersion` string | Pre: none. Post: the publisher records the source address against the device identity and begins sending. A device is **forgotten after 6 s** without a registration — three missed keepalives, which tolerates a brief stall. Idempotent: repeats refresh the entry, never duplicate it |
| `EVT-FRAME` | Telemetry frame | PC → device, adaptive cadence · `COMPONENT-PUBLISHER` · serves `CAP-GEAR`, `CAP-SHIFT`, `CAP-SPOTTER-LEFT`, `CAP-SPOTTER-RIGHT`, `CAP-LINKSTATE` | `protocolMajor` int · `protocolMinor` int · `status` one of `live` `noSim` `unsupportedTitle` `adapterFault` · `titleId` string or `null` · `stamp` int, ms within a producer run · `gear` string `R` `N` `1`–`18` or `null` · `rpm` number ≥ 0 or `null` · `rampStartRpm` number or `null` · `flashRpm` number or `null` · `spotterLeft` one of `none` `one` `two` or `null` · `spotterRight` same | Pre: the device has registered. Post: none — fire-and-forget, unacknowledged and lossy by design. Bounds are not restated here: the gear-domain, threshold-order, RPM and status-consistency invariants own them and this projection defers to them |

### Output documents

| ID | Document | Emitted by · consumed by | Schema |
|---|---|---|---|
| `OUT-CAPTURE` | Capture file | `COMPONENT-PUBLISHER` · `COMPONENT-REPLAY` | **The schema is owned by Domain & Data as `ENTITY-CAPTURE` and is not restated here.** What this concern owns is the wire-facing fact that its `frame` member is a verbatim `EVT-FRAME` payload, so the two serialisers — written in different languages — must agree with the same message contract rather than with each other. **No stability commitment is made to anyone outside the repository**: capture and replay are development tooling, per Product & Requirements, and the format may change with the tooling |

### API elements

| ID | Element | Owner · serves | Request | Response projection | Errors · side-effects |
|---|---|---|---|---|---|
| `API-STATE` | `GET /state` on the device's LAN address | `COMPONENT-WEB` · serves `CAP-LINKSTATE` and the test surface | No parameters, no body — so the bar's default-and-empty-input rule is satisfied vacuously and is recorded as such | JSON projection of `ENTITY-DISPLAYSTATE`: `gearGlyph` string or `null` · `shiftPhase` string · `rampPosition` number 0–1 or `null` · `barLeft` bool · `barRight` bool · `linkState` string **or `null` while the driving screen is showing** · `configuredHost` string · `lastFrameAgeMs` int or `null` · and **five** counters, each an unsigned integer counted **since boot and not persisted**: `malformedCount` · `fieldRangeCount` · `outOfOrderCount` · `versionRejectedCount` · `oversizedCount`.<br><br>**Plus seven diagnostic fields added 2026-09-22/23** under the additive rule below, which consumers must ignore if unknown: `firmwareVersion` · `deviceId` — so a failing test run can say which binary it was talking to; `lastDrawUs` · `worstDrawUs` · `drawCount` — render timing, which is how the shift-cue tearing was diagnosed after two reasoned fixes had failed; `uptimeMs` · `resetReason` — which distinguish an intended restart from a crash, a watchdog or a **brownout**, the last being a power-supply fault that is otherwise indistinguishable from a firmware one · **`backlightOn` bool**, added 2026-09-23 — whether the panel is currently lit. This is not a convenience: `CAP-BLANK` makes a working panel and a dead one look identical from the driving seat, and this field is what distinguishes them. It is reachable while the glass is dark, which is precisely when it is needed. `blankAfterMinutes` is also reported, so the configured period can be read without opening the form | No authentication, so no auth error. A malformed request path returns the uniform error rendering, never a raw framework page. **Read-only: no side-effects of any kind** |

### Data-entry surfaces

`UIF-*` is a **locally-coined prefix**, which the standard permits: it resolves because it is
defined here and referenced consistently across the set. The registry's suggested `BIND`/`DOM`
schemes describe input bindings and test hooks below a screen, which is not what these are — these
are form data-entry contracts.

| ID | Surface | Owner · serves | Fields | Validation · empty semantics |
|---|---|---|---|---|
| `UIF-PORTAL` | Provisioning form, served from the device's own access point | `COMPONENT-WEB` · serves `CAP-PROVISION` | `ssid` · `wifiPassword` · `pcHost` · `credential` | All required; **empty is always a validation failure**, never a default. SSID at most 32 octets; passphrase 8–63 characters; host a dotted-quad or a DNS name of at most 253 characters. On submit the device runs the verification sequence below and persists only on success |
| `UIF-CONFIG` | Configuration page, served on the device's LAN address behind the credential | `COMPONENT-WEB` · serves `CAP-RECONFIG` | the same four fields, pre-filled except secrets, **plus `blankAfterMinutes`** (integer 0–120, pre-filled with the stored value, `0` meaning never blank), plus soft-fault counters and an erase action | Same rules, with one labelled exception: a **blank secret field means leave unchanged**, stated on the form itself. Erase requires confirmation. A `blankAfterMinutes` outside 0–120, or one that is not a whole number, is an `ERR-PORTAL-INPUT` and re-renders the form with the field marked — it is never silently clamped, because a clamped value would leave the operator believing they set something they did not.<br><br>**On a failed network change the device raises the provisioning portal. It never rolls back and never erases.** Resolved by operator decision 2026-09-23; this replaces the gap marker that stood here. The argument is that a device *cannot distinguish a wrong passphrase from a router that is briefly down*, so any automatic rollback is a guess — and a guess that can discard a correct setting because the network happened to be rebooting. Raising the portal puts the choice in front of the person who knows which it was, and reuses the recovery surface an unprovisioned device already uses rather than adding a second mechanism.<br><br>The WiFi credentials are handed to the platform's own WiFi store rather than copied into `ENTITY-DEVICECONFIG`, so the passphrase exists in one place rather than two. Changing network requires its password: a blank one means *unchanged*, which cannot apply to a different network, and silently joining an open network would be a worse outcome than asking |

**Portal verification sequence**, each step reporting its own failure and each with a stated bound.
Nothing is persisted until all four succeed.

| Step | Bound | Why this value |
|---|---|---|
| Join WiFi | 20 s | Covers a slow router or a weak signal at the rig; a shorter bound fails against a network that would have worked |
| Resolve host | 8 s | Enough for a sluggish DNS responder, and it distinguishes a name problem from an address problem |
| Register | included in the frame wait | The device cannot observe whether a datagram arrived; only the reply proves it |
| Receive a frame | 15 s | The binding constraint: with no sim running the plugin sends at roughly **1 Hz**, so a short wait would report failure against a perfectly healthy PC sitting in the menus |

The generous values are deliberate. This sequence runs once, while a person is standing there able
to fix things, and a false failure at that moment is far more costly than fifteen seconds.

### Error catalogue

Classification per `PATTERN-ERROR`: **hard** faults each get a screen; **soft** faults become named
counters on the configuration page. No row terminates in serial output, and every row reaches a
human-visible surface. `ERR-PUBLISH-FAILED` is the one **PC-side** row and its surface is SimHub's
log — classified apart above, for a cause rather than by choice.

| ID | Condition | Class · surface | Forced by · tier |
|---|---|---|---|
| `ERR-VERSION-MAJOR` | Frame or registration whose major version is not accepted | hard · version-mismatch screen naming both versions | Replay tool emits a bumped major · integration |
| `ERR-MALFORMED` | Datagram that is not parseable JSON, or is missing a required key | soft · counter | Replay tool emits truncated and non-JSON payloads · integration |
| `ERR-FIELD-RANGE` | A field violating an owned invariant — inverted thresholds, negative RPM, gear outside its domain, a value present on a non-live frame | soft · counter | Replay tool emits crafted frames, one per invariant · integration |
| `ERR-OUT-OF-ORDER` | Frame whose stamp does not exceed the newest seen, and is not a producer restart | soft · counter | Replay tool transposes two frames · integration |
| `ERR-LINK-SILENT` | No frame for longer than the staleness threshold | hard · link-state screen | Stop the replay tool mid-stream · integration |
| `ERR-HOST-UNRESOLVED` | The configured host cannot be resolved | hard · distinct screen naming the configured host | Configure a non-existent DNS name · integration |
| `ERR-WIFI-LOST` | Association lost, or never established | hard · distinct screen; recovery per `PATTERN-RECONNECT` | Disable the access point · integration |
| `ERR-CONFIG-UNREADABLE` | Stored record corrupt, or more than one schema version old | hard · portal raised, stating that stored settings could not be read | Write a deliberately bad record, then boot · integration |
| `ERR-UNSUPPORTED-TITLE` | A sim is running for which no adapter exists — arrives as a frame status, not as a transport failure | hard · screen naming the title | Run a sim with no adapter, or replay a frame with that status · integration |
| `ERR-ADAPTER-FAULT` | An adapter threw and the plugin caught it at its boundary — arrives as a frame status | hard · screen | Stub adapter that throws on every update · integration |
| `ERR-PORTAL-INPUT` | A portal or configuration field fails validation | hard · re-rendered form with the offending field marked | Submit each invalid case · integration |
| `ERR-PORTAL-VERIFY` | The portal's verification sequence fails at a named step | hard · form states which step failed and what to check | Submit a wrong passphrase, then a wrong host, then with the plugin stopped · integration |
| `ERR-AUTH-FAILED` | Incorrect credential presented to the configuration page | hard · page rejects and re-prompts | Submit a wrong credential · integration |
| `ERR-UNKNOWN-PATH` | A request to any HTTP path the device does not serve, on either the portal or the LAN surface | hard · the uniform error rendering, never the framework's own default page | Request an unrouted path · integration |
| `ERR-OVERSIZED` | A datagram larger than the 2 KB input bound arrives and is discarded **before parsing** | soft · counted only, no screen — it is not a condition the driver can act on | Replay tool emits an oversized payload · integration. **Note `S8`**: on this hardware nothing above 1472 bytes reaches the application at all, so the guard cannot fire and the counter cannot increment. The row exists because the counter is human-visible on the configuration page and every visible counter owes an owner |
| `ERR-PUBLISH-FAILED` | The publisher's socket cannot send | hard, on the PC side · logged once per cause in SimHub's log | Bind the port in another process, then start the plugin · integration |

The last row is the one whose surface is on the PC rather than the panel, and it is recorded as a
**known limitation**: from the device's side it is indistinguishable from the plugin not running,
because there is no acknowledgement path. The frame-status mechanism cannot help, since the failure
is the inability to send at all.

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| I1 | A live frame captured off the wire carries every field named in the frame projection, in camelCase, and is readable without a schema to hand | `EVT-FRAME` projection |
| I2 | For each telemetry element in turn, a frame carrying `null` renders that element as absent, distinguishably from the element's negative value | the null-means-unavailable convention |
| I3 | A frame omitting a required key is rejected as malformed rather than treated as unavailable | the same convention, inverse case |
| I4 | With status live the plugin sends at SimHub's update rate; with any other status it sends at approximately 1 Hz | adaptive cadence |
| I5 | A frame with the same major, a higher minor and an unknown field is accepted and renders normally | version tolerance |
| I6 | A frame with a different major is refused whole, shows the mismatch screen naming both versions, and registration continues | `ERR-VERSION-MAJOR` |
| I7 | A device that stops registering is dropped after 6 s, and resumes receiving within one interval of registering again | `EVT-REGISTRATION` lifecycle |
| I8 | Repeated registrations from one device produce exactly one table entry | registration idempotency |
| I9 | A registration whose device identity does not match the owned format is rejected | validator honours the owned identifier format |
| I10 | Every row of the error catalogue is provoked by its named forcing, and each produces its stated screen, form message or counter; none is observable only on serial. `ERR-PUBLISH-FAILED` is asserted against its PC-side log surface, and `ERR-UNKNOWN-PATH` against the uniform rendering on both HTTP surfaces | error-model totality and the never-console-only rule |
| I11 | Each portal field is submitted empty and rejected; the configuration page's secret fields left blank leave the stored secrets unchanged | empty-value semantics, including the labelled exception |
| I12 | The portal's verification sequence is failed at each of its four steps in turn, each reports that step specifically, and nothing is persisted on any failure | `ERR-PORTAL-VERIFY`, `UIF-PORTAL` postcondition |
| I13 | `GET /state` returns a JSON projection whose values match what the panel is displaying at that instant, requires no credential, and is present in a release build | `API-STATE` |
| I14 | An unknown path under the device's HTTP root returns the uniform error rendering rather than a framework default page | error-model totality across the HTTP surface |
| I15 | Each of the four counters increments only on its own fault class, and all four read zero after a reboot | counter semantics, not persisted |
| I16 | Each step of the portal verification sequence is held past its stated bound and reports that step's failure at that bound, not earlier or later | the verification bounds |

