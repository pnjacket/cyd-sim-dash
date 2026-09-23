---
artifact: product-doc
role: concern
concern-id: security-and-privacy
behavior: baseline
trigger: always (raised by security_risk: secrets, network)
in-scope-subaspects: [trust-boundaries, secrets-credential-handling, authentication-mechanism, authorization, threat-model, encryption, data-protection-mechanisms-per-sensitive-field]
current-rung: contract-grade
status: draft
version: 0.5.0
---

# Security & Privacy — cyd-sim-dash

> A LAN-trusting device holding your WiFi passphrase, with a firmware-update path that opens on request.

## Purpose & Scope

The risk is small but not zero, and it concentrates in two places: the device **stores the user's
WiFi passphrase**, and it **accepts a firmware update over the network**. A dashboard showing gear
numbers is not a valuable target; a device inside your house that someone on your network can
reflash is a different proposition.

This concern's entire Contract-grade surface is **`SEC-*` assertions** — boundary, hygiene and
negative statements. The standard sanctions that shape explicitly for a minimal-risk product, and
it is the honest form here: there are no roles to model and no permissions to grant, so what is
owed is a precise statement of what *is* and *is not* defended, stated contractually rather than
left implicit.

## Non-goals / Out-of-scope

- **`session-management`** — *absent*. No sessions exist. The credential is presented per request;
  nothing is remembered between them. No re-entry note owed.
- **Physical-access protection** — *deferred*. Storage is unencrypted; anyone with the device and
  a USB cable can read the WiFi passphrase. [FUTURE-SCOPE] Re-entry is ESP32 flash encryption,
  which is irreversible once enabled, complicates OTA and development flashing, and can brick a
  board if misconfigured — a heavy mechanism for this product, deliberately not taken.
- **Firmware image signing** — *deferred*. Beyond the framework's own transfer checks, nothing
  verifies an OTA image's provenance. [FUTURE-SCOPE] Re-entry alongside signing infrastructure.
- **Confidentiality of telemetry** — *absent*. Gear, RPM and proximity have no confidentiality
  value, so nothing protects them anywhere: not on the wire, not on the state endpoint, not in a
  capture. No re-entry note owed.

## Requirements

### Trust boundaries

**The home LAN is the trust boundary, and everything inside it is trusted.** That is the posture,
stated plainly, and three separate decisions follow from it rather than each being argued
independently: registration is unauthenticated, telemetry frames are accepted from any source, and
the state endpoint is open. Consistency here is a feature — a product that defended one of these
and not the others would be harder to reason about, not safer.

Two surfaces sit *outside* that posture because they can change what the device is and does rather
than merely read it: the configuration page and the OTA path. Both are gated by the device
credential.

One consequence follows and is recorded rather than glossed: because the LAN is trusted, the
credential is transmitted to the configuration page **in the clear over plain HTTP**. That is
consistent with everything else crossing the same LAN unencrypted; it is a derived consequence of
the boundary decision, not an independent choice.

### Secrets & credential handling

Two secrets exist, both supplied by the user through the portal and both persisted on the device:
the **WiFi passphrase** and a **device credential** that gates the configuration page and the OTA
path alike — one secret, not two.

Their confinement rule is owned by Domain & Data as an invariant, and is **advisory** there because
no schema enforces it. What this concern owns is the mechanism: where each is stored, in what form,
who can read it, and how it is destroyed. That table is in *Contracts*.

**The credential has no minimum length** — only a non-empty requirement. The consequence is worth
stating once and precisely, because two decisions compound: the OTA path is closed except during a
window, but **the window is opened from the configuration page**, which is always listening and
gated by that same credential. So the window's benefit is bounded by whatever credential is chosen.
A trivial credential means anyone on the LAN can open the window and flash the device.

### Authentication mechanism

No user authentication exists; there are no users. What exists is a **single shared device
credential** gating two surfaces:

- The **configuration page**, always listening while the device is connected, because its purpose is
  to be reachable when the portal is not.
- The **OTA update path**, which listens **only during a window** opened deliberately from the
  configuration page and closing after **30 minutes**. For most of the device's life there is no
  firmware-flashing path listening at all. Thirty minutes is long enough to build and upload
  without racing a clock, and short enough that the surface is closed almost always.

The credential must be non-empty; an empty value is rejected by both forms and no default is
shipped.

### Threat model

Deliberately light. Per the concern spec, its job here inverts: this is where "X is not a threat"
becomes **contractual** rather than unexamined. Those are the negative `SEC-*` assertions in
*Contracts*. The realistic threats, in rough order:

1. **A weak credential on an always-listening configuration page**, which is the path to opening
   the OTA window and therefore to arbitrary firmware on hardware in your house. The highest-value
   target here by a distance, and the one the no-minimum decision leaves open.
2. **WiFi passphrase recovery from a device that is lost, sold or discarded.** Mitigated by the
   erase action, not by storage protection.
3. **A malformed datagram crashing the firmware.** A robustness problem with a security shape: any
   device on the LAN can send one, and the JSON parser is the only place an untrusted party reaches
   this product's code. This is not hypothetical — ArduinoJson carried exactly such a defect through
   version 7.4.2, which is why `SEC-PARSER-FLOOR` exists and why the dependency's version is a
   security control rather than housekeeping.

Threat 3 is the one that receives a real mechanism, because it is the only one reachable without
either physical access or the credential.

### Encryption

- **In transit: none.** UDP frames, the state endpoint, the portal, the configuration page and the
  credential itself all cross the LAN in the clear. Deliberate, and consistent with the trust
  boundary.
- **At rest: none.** Secrets sit in plain NVS. The exposure — physical access plus a cable yields
  the WiFi passphrase — is accepted and named rather than left for someone to discover.
- **OTA image: transfer-checked only.** No signing, no provenance verification beyond what the
  framework performs.

### Data-protection mechanisms per sensitive field

One mechanism per classified field, in *Contracts*, covering storage form, readers, transmission
and destruction. The destruction column is the one that matters for the case the erase action
exists to serve: **values are overwritten before their keys are deleted**, because plain NVS can
otherwise leave the old bytes recoverable in flash — which would make the erase action a gesture
rather than a mechanism.

## Open Questions

- [GAP] Recovery has a dead end worth naming, though no decision is taken here: erase is reachable
  only from the configuration page, and the OTA window is opened only from the configuration page.
  If that page becomes unreachable — forgotten credential, or a device on a network you cannot
  join — neither erasing nor updating is possible and USB reflashing is the only route. Each
  decision is individually reasonable; together they close every network path. Recorded for
  adjudication rather than resolved, since the relevant choices are already made.
- [REVISIT] Whether the credential should be sent as a challenge rather than in the clear, if the
  LAN-trusted posture is ever narrowed.

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Domain & Data | the classification tags naming which fields are sensitive, and the advisory confinement invariant whose mechanism this concern owns |
| Interfaces & Contracts | the event, API and data-entry surfaces that the authorization table below covers, one row each |
| Architecture | the trust boundaries the component model draws, and the error pattern that routes a rejected datagram to a counter |
| Product & Requirements | the adopter persona, whose device is the one being protected from its own owner's mistakes |
| Quality & Testing | checks Q5 through Q8, which prove the assertions below |

## Examples / Worked scenarios

**A hostile datagram.** A compromised device on the LAN sends a 60 KB payload to the telemetry
port. The device rejects it on size before the parser sees a byte, increments a counter, and
continues. Nothing is allocated and nothing is parsed.

**A spurious but well-formed frame.** The same device sends a valid frame claiming eighth gear. The
device accepts and displays it, because frames are accepted from any source. This is a real
consequence of the LAN-trusted posture and is asserted rather than defended against.

**Selling the device.** The owner opens the configuration page, runs erase, and confirms. Stored
values are overwritten and their keys deleted, so a subsequent flash dump does not yield the WiFi
passphrase. The device boots unprovisioned and raises its portal.

**A forgotten credential.** The configuration page cannot be reached, so neither erase nor the OTA
window can be opened. The device continues working exactly as before; changing anything requires a
USB cable. See Open Questions.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| The LAN is trusted, uniformly | One coherent posture is easier to reason about than three partial defences; the telemetry has no confidentiality value | Anyone on the LAN can register, drive the panel with forged frames, and read the state endpoint |
| Secrets in plain NVS | Flash encryption is irreversible, complicates OTA and development, and can brick a board — heavy for a gear display | Physical access plus a cable yields the WiFi passphrase |
| Credential non-empty, no minimum | Minimum friction at setup, which is where an adopter is most likely to give up | A trivial credential is permitted on the surface that can open the OTA window |
| OTA listens only in a window | Removes the firmware-flashing path for most of the device's life | The window is opened from the always-listening configuration page, so its benefit is bounded by the credential above |
| Frames accepted from any source | Simpler, and it lets the replay tool run from any machine without matching the configured host | Anyone on the LAN can drive the panel |
| Erase overwrites before deleting | Plain NVS can leave deleted values recoverable, which would make erase a gesture | A few lines of code |
| Reject datagrams above 2 KB into a fixed buffer | The parser is the only place an untrusted party reaches this code; a fixed buffer means no allocation on the receive path | Permanently held RAM on a board with no PSRAM, and a cap to revisit if the frame grows |

## Contracts

### Security assertions

| ID | Assertion | Kind |
|---|---|---|
| `SEC-LAN-TRUSTED` | The home LAN is the trust boundary. Every host on it is trusted for every read-only interaction with this product | boundary |
| `SEC-NO-EGRESS` | Neither component contacts anything outside the LAN. No telemetry, no analytics, no update check, no cloud service, in any build | negative, verifiable |
| `SEC-NO-PERSONAL-DATA` | No personal data is collected, stored or transmitted by either component | negative |
| `SEC-NO-SUBSCRIPTION-AUTHZ` | Registration is unauthenticated. Any host on the LAN may register and will be sent frames | negative, asserted deliberately |
| `SEC-NO-SOURCE-CHECK` | Telemetry frames are accepted regardless of source address | negative, asserted deliberately |
| `SEC-STATE-OPEN` | The state endpoint requires no credential and is present in release builds | negative, asserted deliberately |
| `SEC-CREDENTIAL-POLICY` | One shared credential gates the configuration page and the OTA path. It must be non-empty. No minimum length and no composition rule is imposed, and no default is shipped | hygiene |
| `SEC-OTA-WINDOW` | The OTA path listens only during a window opened from the configuration page, closing **30 minutes** after it is opened. Outside that window no firmware-flashing path is listening | hygiene |
| `SEC-OTA-IMAGE` | OTA images are checked only by the framework's own transfer integrity. No signing or provenance verification is performed | negative |
| `SEC-STORAGE-PLAIN` | Secrets are stored unencrypted in NVS. Physical access with a cable yields them | negative, exposure named |
| `SEC-ERASE-OVERWRITE` | The erase action overwrites stored values before deleting their keys | hygiene |
| `SEC-INPUT-BOUND` | Datagrams larger than 2 KB are rejected before parsing. The receive path uses a fixed buffer and performs no dynamic allocation | hygiene |
| `SEC-PARSER-FLOOR` | ArduinoJson is pinned at **7.4.3 or later**. Every version through 7.4.2 carries a buffer overrun in string-to-float conversion, reachable by a JSON string of many digits — which is exactly what an untrusted host on the LAN can send, and the 2 KB cap does not close it because 2 KB of digits is ample | hygiene, with a named upstream cause |
| `SEC-TRANSIT-CLEAR` | Nothing is encrypted in transit, including the credential presented to the configuration page | negative, derived from `SEC-LAN-TRUSTED` |

### Per-interface authorization

The bar requires a row per interface element, including the event surface. Every row is "none"
except two — and the nones are contractual assertions, not omissions.

| Surface | Element | Required credential | Notes |
|---|---|---|---|
| event, inbound | `EVT-REGISTRATION` | none | Per `SEC-NO-SUBSCRIPTION-AUTHZ`. The publisher replies only to the source address of a registration it received, so it cannot be induced to send traffic elsewhere |
| event, outbound | `EVT-FRAME` | none | Per `SEC-NO-SOURCE-CHECK` |
| HTTP read | `API-STATE` | none | Per `SEC-STATE-OPEN` |
| form, on the device's own AP | `UIF-PORTAL` | none | Possession of the access point is the boundary |
| form, on the LAN | `UIF-CONFIG` | device credential | Checked per request; no session is established |
| firmware update | OTA path | device credential **and** an open window | Per `SEC-OTA-WINDOW` |

**No `ROLE-*` is minted.** There are no roles, tenant-scoped or resource-scoped, because there are
no users and no per-resource relationships. The standard permits a role-free authorization model;
the absence is recorded here so it reads as a finding rather than an oversight.

### Protection mechanism per sensitive field

| ID | Field · tag | Storage | Readers | Transmission | Destruction |
|---|---|---|---|---|---|
| `SEC-FIELD-WIFI` | WiFi passphrase · `secret` | Plain NVS | Firmware only | Never transmitted in any message, rendered on any screen, written to serial, or included in a capture | Overwritten then deleted on erase |
| `SEC-FIELD-CREDENTIAL` | Device credential · `secret` | Plain NVS | Firmware only; compared per request | Received in the clear from the configuration page; never sent outward | Overwritten then deleted on erase |
| `SEC-FIELD-IDENTIFYING` | SSID, PC host, device identity · identifying | Plain NVS, except device identity which is derived from hardware and not stored | Firmware; the host is shown on the configuration page by design | Excluded from captures so a fixture can be shared publicly | Overwritten then deleted on erase, except the derived identity which cannot be erased |

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| S1 | A packet capture across a full session, including a configuration-page login, shows no traffic leaving the LAN and no external DNS lookup — and shows the credential and telemetry in plaintext, confirming the transit posture rather than assuming it | `SEC-NO-EGRESS`, `SEC-TRANSIT-CLEAR` |
| S2 | A host other than the configured one registers and receives frames; a frame from an arbitrary source is accepted and displayed | `SEC-NO-SUBSCRIPTION-AUTHZ`, `SEC-NO-SOURCE-CHECK` — asserting the contracted absence so that adding a control later is a deliberate change |
| S3 | The state endpoint responds without a credential on a release build | `SEC-STATE-OPEN` |
| S4 | An empty credential is rejected by the portal and the configuration page; a single-character one is accepted | `SEC-CREDENTIAL-POLICY`, including its deliberate permissiveness |
| S5 | With no window open, an OTA attempt is refused; after opening a window it succeeds; 30 minutes after opening it is refused again | `SEC-OTA-WINDOW` |
| S6 | The configuration page rejects an incorrect credential and establishes no session — a second request without the credential is also rejected | `UIF-CONFIG` authorization, absence of sessions |
| S7 | Flash is dumped twice: after provisioning, where both secrets are recoverable in plaintext — confirming the named exposure rather than assuming it — and again after erase, where neither is | `SEC-STORAGE-PLAIN`, `SEC-ERASE-OVERWRITE`, `SEC-FIELD-WIFI`, `SEC-FIELD-CREDENTIAL` |
| S8 | Datagrams of 2 KB, just over 2 KB, and 60 KB are sent; the device neither crashes nor reboots, and any oversized datagram that *is* delivered is rejected before parsing. **Amended 2026-09-22 against measurement:** nothing above **1472 bytes** — the largest UDP payload fitting one 1500-byte MTU frame — is ever delivered to the application on this hardware; the platform drops it below the firmware. The original wording also required the rejection to be *counted*, which this platform makes unobservable: the guard cannot fire, so its counter cannot increment. The guard is kept as defence-in-depth, correct on any platform that reassembles and costing one comparison, and the 2 KB boundary itself is asserted by the unit tier instead | `SEC-INPUT-BOUND` |
| S9 | A corpus of malformed payloads — truncated, wrong types, deeply nested, duplicate keys, missing keys, **and a numeric string of several hundred digits** — leaves the device running with every one counted | `SEC-INPUT-BOUND`, malformed-input hardening |
| S12 | The built firmware links ArduinoJson 7.4.3 or later, asserted from the build rather than from documentation | `SEC-PARSER-FLOOR` |
| S10 | Serial output, every screen, the state endpoint and a capture file are inspected across a full provisioning and driving cycle; no secret appears in any of them | the confinement invariant's mechanism, advisory |
| S11 | A capture file taken from a provisioned device contains no SSID, host or device identity | `SEC-FIELD-IDENTIFYING` |

