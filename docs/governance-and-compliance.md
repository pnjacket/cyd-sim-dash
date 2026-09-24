---
artifact: product-doc
role: concern
concern-id: governance-and-compliance
behavior: baseline
trigger: always (source-provenance scaled by code_authorship: model-authored)
in-scope-subaspects: [license-ip-compliance, source-provenance]
current-rung: contract-grade
status: published
version: 0.6.0
---

# Governance & Compliance — cyd-sim-dash

> MIT outbound, an LGPL core underneath, and a repository that will be largely written by a model in a genre where example code circulates by copy-paste.

## Rung note

This concern went down to `specified` after the first 2026-09-21 audit and back to `contract-grade`
after the second. The first raise was premature: only the two rows that audit had named were
cleared, and the register's own Status column still read *Researched* for four others — which the
second audit correctly called out as the bar's literal words going unmet.

All eight rows are now **Cleared** against primary sources, the register states its population rule
and its two-state vocabulary so completeness is judgeable, and TFT_eSPI's three licences are
recorded rather than flattened to one.

The provenance register is still empty, which is correct: it is created with the first firmware
file. An empty register before any code exists is not an uncleared one.

## Purpose & Scope

Two things only: **which licences apply**, inbound and outbound, and **where the first-party code
came from**. Both matter more here than the project's size suggests, for a specific reason — CYD
firmware is a genre in which example code circulates by copy-paste, and this repository is
model-authored, which is exactly the combination the source-provenance sub-aspect exists for.

Business & Legal is scoped out of the set, so the one constraint it would have owned — that no
copyleft source enters this project's own code — is folded in here as a policy, per the intake
agreement and per the standard's allowance that an IP-compliance policy carries the `POLICY-*`
prefix like any other.

**On that concern's trigger, which has two halves.** The product is *not* commercial, but it **is**
distributed, and a declared don't-derive-from constraint activates Business & Legal in its own
right even for a non-commercial product. The honest position is therefore not "the trigger did not
fire" but "it fired and was answered here": the constraint's **enforcement** lives in
`POLICY-NO-COPYLEFT-SOURCE`, and the facts it would otherwise state as a `LEGAL-*` contract — which
upstream, which licence, why incompatible — are recorded in the inbound register below rather than
in a concern of their own. `eula-tos` is genuinely absent: source distributed under MIT carries its
licence text and nothing further is offered or required.

**Nothing here is legal advice.** These are recorded determinations about how the product is
distributed and what obligations follow, made by the operator and written down so they are
reviewable rather than implicit.

## Non-goals / Out-of-scope

- **`data-handling-policy`** — *deferred*. Classified fields do exist — Domain & Data tags two
  `secret` and three identifying — so the subject is present and the earlier `absent` claim was
  wrong. What is calibrated out is the **policy layer above the mechanism**: Security & Privacy
  already states protection, destruction and confinement per field, and a policy restating them
  would govern nothing today. [FUTURE-SCOPE] Re-entry if the product ever handles data belonging to
  someone other than its own user, at which point the tag → policy → mechanism chain needs its
  middle link.
- **`audit-requirements`** — *absent*. Nothing is accountable to anyone; single user, no tenancy,
  no audit trail to retain. No re-entry note owed.
- **`compliance-framework-mapping`** — *absent*. Unregulated, and no personal data is processed, so
  no framework obligation attaches. No re-entry note owed.
- **`records-retention-data-residency`** — *absent*. No records are retained and everything lives
  on one LAN. No re-entry note owed.
- **`consent-management`** — *absent*. No personal data, so no consent basis exists. No re-entry
  note owed.
- **Prebuilt firmware binaries are not distributed** — *deferred*. v1 ships source only, which is
  what keeps the LGPL obligation trivial. [FUTURE-SCOPE] Re-entry is a real decision with real
  work attached: shipping a binary means distributing a combined work containing LGPL-2.1 code,
  which brings relink obligations — typically met by publishing object files or complete build
  inputs alongside each release.

## Requirements

### License & IP compliance

**Outbound stance: MIT**, chosen at intake on the expectation of public sharing. The stance is a
contract in its own right, not merely an absence of restriction.

**The inbound set is not uniformly permissive, and one dependency is the reason this section is
not a formality.** The register is in *Contracts*; the finding that matters:

> The **ESP32 Arduino core is LGPL-2.1**, not a permissive licence.

What that does and does not mean here, recorded so it is not re-derived under pressure later:

- It does **not** affect the licence of this product's own source. First-party code remains MIT.
- It **does** attach obligations to a distributed **combined work** — a compiled binary containing
  the core — principally that a recipient must be able to relink the product against a modified
  core.
- Because **v1 distributes source only**, the adopter performs the linking on their own machine.
  There is no combined work distributed by this project, and the obligation is satisfied by
  construction rather than by effort.

This is the entire reason the source-only decision is recorded as a governance matter rather than
a packaging preference. If a prebuilt binary is ever shipped, this section changes.

A **third-party notice file** is carried in the repository listing each dependency, its version and
its licence — cheap now, and already the file that a binary release would require.

**SimHub is proprietary, and the arrangement that keeps this clean is now recorded concretely.** A
plugin project references exactly three assemblies — `GameReaderCommon.dll`, `SimHub.Logging.dll`
and `SimHub.Plugins.dll` — from the builder's **own SimHub installation**, resolved through a
`SIMHUB_INSTALL_PATH` environment variable (default `C:\Program Files (x86)\SimHub\`). Each
reference is marked **`Private="False"`**, so the assemblies are **not copied into the build
output** and therefore cannot end up in a release artefact. The built plugin DLL is placed into the
SimHub folder, which is also where SimHub loads it from. SimHub ships its SDK demos at
`C:\Program Files (x86)\SimHub\PluginSdk`.

That `Private="False"` setting is the whole mechanism: redistribution is prevented by the build
configuration rather than by anyone remembering. Check G3 asserts it from the other direction, by
inspecting the artefact.

### Source provenance (first-party code)

The `code_authorship` trait is **model-authored**, which the standard treats as meaning provenance
is **undeclared by default**: a model can reproduce licensed source with no attribution, and
nobody involved need realise. A best-effort detection pass is therefore owed, and — this is the
part that is easy to skip — **its residual is recorded rather than read as "cleared"**.

Three mechanisms, specified in *Contracts*:

1. **In-code attestation.** Every non-trivial first-party unit copied, ported or adapted from an
   external source carries a `SOURCE:` marker at its code site naming the origin and licence. The
   marker travels with the code through refactors and file moves.
2. **A register file** collecting those attestations with an outbound-compatibility determination
   per unit. The markers are where the truth lives; the file is what a reviewer actually reads.
3. **A detection pass**, because attestation alone reproduces exactly the blindness it exists to
   close. It runs **before the first public push, and again before each release that added code** —
   tying the obligation to the moment distribution actually happens.

**Where the pass looks.** Targeted rather than exhaustive: the units most likely to be
reproductions. In this product that is a short and predictable list — display initialisation and
pin configuration above all, since nearly every working CYD project derives its display setup from
a small number of circulating examples, plus portal handling and any JSON parsing helpers.

**What it cannot do**, stated rather than implied: a targeted search finds recognisable
reproductions of *public, distinctive* code. It does not find paraphrased reproductions, code from
sources not publicly indexed, or short idiomatic fragments that are reproductions in fact but
unremarkable in form. A green dependency scan would add nothing to this — it is structurally blind
to code copied into first-party source, having no package and no import edge to follow.

**On a finding**: an incompatible unit is a **defect**, not a smell. It is removed and
reimplemented from the specification rather than from the source it was found to match. No
per-finding incident log is kept (operator decision, 2026-09-21) — the residual statement below is
the record that the pass ran and what it could not reach.

## Open Questions

- ~~WiFiManager's licence.~~ **Cleared 2026-09-21** — MIT, read from the repository's `LICENSE`.
- ~~The SimHub assembly-referencing arrangement.~~ **Resolved 2026-09-21** — three `Private="False"`
  references resolved through `SIMHUB_INSTALL_PATH`.
- ~~No versions recorded.~~ **Resolved** — pinned by decision in Integrations; the notice file can
  be written now and the CI workflow enforces the same numbers.

## Dependencies & Cross-references

| Consumed from | What |
|---|---|
| Integrations | the dependency set whose licences this concern clears, and the version table the notice file mirrors |
| Architecture | the technology choices that determined the inbound set, including the LGPL-2.1 core |
| Product & Requirements | the MIT stance and the publish-v1-as-is decision that makes distribution real rather than hypothetical |
| Quality & Testing | the checks that prove the register is complete and the pass was run |
| Delivery Process | the release step at which the pass and the notice file are obligations |

## Examples / Worked scenarios

**A reproduction found.** The detection pass searches a distinctive line from the display
initialisation and matches a widely-circulated CYD example carrying no licence header. The unit is
a defect: it is removed and the initialisation rewritten from the datasheet and the library's own
documentation. The register records the determination.

**A reproduction declared rather than found.** A unit was adapted from a blog post and the author
marked it at the code site with its origin and licence. The register carries the
outbound-compatibility determination. This is the declared case, and it is the easy one.

**The case neither mechanism closes.** A short helper was reproduced from a source nobody
recognised, carries no marker because nobody realised, and is too unremarkable for a search to
surface. It remains undetected. This is the residual, and it is recorded as such rather than
described as cleared.

**Someone asks what licences the firmware pulls in.** They read the notice file, see the LGPL-2.1
core alongside the permissive libraries, and see the recorded determination explaining why
source-only distribution makes that unproblematic.

## Design Decisions

| Decision | Rationale | Consequence accepted |
|---|---|---|
| Source-only distribution for v1 | The adopter links the LGPL core themselves, so no combined work is distributed and the obligation is met by construction | Every adopter needs the toolchain — already the documented path, but a barrier for a non-technical one |
| MIT outbound, with the no-copyleft-inbound policy folded in from Business & Legal | Keeps the stance true and gives the constraint an owner after that concern was scoped out | A copyleft reproduction becomes a build-blocking defect rather than a licensing debate |
| Markers plus a register file | Markers survive refactors; the file is readable. Either alone is weaker | Two places to keep in step |
| Targeted detection, not exhaustive | Aimed where CYD code actually circulates; exhaustive search of a model-authored repo is not achievable | A real residual, recorded rather than papered over |
| Pass before first push and per release | Ties the obligation to distribution, which is when it matters | Code written between releases is unexamined until the next one |
| No per-finding incident log | The residual statement is the record that the pass ran | Less evidence of what any individual pass found |

## Contracts

### Policies

| ID | Policy |
|---|---|
| `POLICY-OUTBOUND-MIT` | The product's own source is licensed MIT. The stance is asserted, not merely unstated, and every inbound dependency is cleared against it before being finalised |
| `POLICY-NO-COPYLEFT-SOURCE` | No GPL or otherwise copyleft source may be copied, ported or adapted into this project's first-party code. Linking against a copyleft *library* is a separate matter governed by that library's terms; this policy is about source entering the repository. A violation is a build-blocking defect |
| `POLICY-CLEARANCE-GATES-SELECTION` | A dependency's licence is verified **before** the dependency is finalised, not after. Discovering an incompatibility once the firmware is written is expensive, and the pressure at that point is to look away |
| `POLICY-NO-REDISTRIBUTION-SIMHUB` | SimHub's assemblies are referenced at build time and never redistributed in this repository or any release artefact |
| `POLICY-PROVENANCE-ATTESTATION` | Every non-trivial first-party unit that is copied, ported or adapted carries a `SOURCE:` marker at its code site, and an entry in the register with its outbound-compatibility determination |
| `POLICY-PROVENANCE-DETECTION` | A targeted best-effort detection pass runs before the first public push and before each release that added code. Its residual is recorded. A dependency licence scan does not substitute for it |
| `POLICY-INCOMPATIBLE-IS-DEFECT` | First-party code found to be copied under a licence incompatible with the outbound stance is removed and reimplemented from specification. It is a defect, not a smell, and it blocks release |

### Inbound licence register

**Population rule**, stated so completeness is judgeable: the register carries **one row per
`DEP-###`** minted by Integrations — eight rows — plus nothing else. Runtime externals that ship no
code into this product still appear, with their licence recorded as not-applicable rather than
omitted, because an absent row and a cleared row must not look alike.

**Status vocabulary**, two states only: **Cleared** means the licence was read from the project's
own licence file or an equivalent primary source, and its compatibility with the MIT outbound
stance determined. **Open** means it was not. There is no third state.

| `DEP-###` | Dependency | Licence | Compatible with MIT outbound | Status |
|---|---|---|---|---|
| ESP32 core | arduino-esp32 **3.3.12** | **LGPL-2.1** | Yes for source-only distribution — see the determination below | **Cleared** — read from the repository's `LICENSE.md`. See the mixed-licence note below |
| TFT_eSPI | Bodmer TFT_eSPI **2.5.43** | **Three licences**: MIT (code derived from Adafruit_ILI9341), BSD (code derived from Adafruit_GFX), and FreeBSD for Bodmer's own work and examples | Yes — all three are permissive | **Cleared** — read from the repository's `license.txt`. The register does not flatten this to one licence, because the notice file must reproduce all three |
| ArduinoJson | ArduinoJson **≥ 7.4.3** | MIT, © 2014–2026 Benoit Blanchon | Yes | **Cleared** — read from the repository's `LICENSE.txt`. The version floor is a security requirement, owned by Integrations |
| WiFiManager | tzapu WiFiManager **2.0.17** | MIT, © 2015 tzapu | Yes | **Cleared** — read from the repository's `LICENSE`. Maintained by tablatronix |
| SimHub SDK | SimHub plugin assemblies | Proprietary | Referenced only, never redistributed | **Cleared** — `Private="False"` references from the builder's own install; nothing is copied to output |
| SimHub | SimHub application | Proprietary | Not applicable — a runtime host the user installs; no code from it enters this product | **Cleared** |
| iRacing | iRacing | Proprietary | Not applicable — a runtime data source reached only through SimHub | **Cleared** |
| Python | Python runtime | PSF | Not applicable — development tooling, not distributed with the product | **Cleared** |

**Mixed-licence note on the ESP32 core.** The repository declares LGPL-2.1, and the distribution
additionally bundles third-party components under their own terms — ESP-IDF components are
Apache-2.0. None is copyleft in a way that reaches first-party source, and the source-only
determination below is unaffected. [GAP] The notice file must reproduce the bundled components'
notices, not only the core's declaration; the exact set is enumerated when the core is installed.

**Recorded determination on the LGPL-2.1 core.** First-party source remains MIT. LGPL obligations
attach to a distributed combined work; v1 distributes source only, so no combined work leaves this
project and the relink obligation is satisfied by the adopter performing the link. Revisit in full
if a prebuilt binary is ever shipped.

### Provenance register

[GAP] The register is empty because no code exists. It is created with the first firmware file
rather than reconstructed later — reconstructing provenance after the fact is close to impossible,
and a register begun late is one that silently omits everything written before it.

Each entry: unit · origin · licence · outbound-compatibility determination · date.

### Residual statement

Required by the standard and reproduced at each pass. The detection pass finds recognisable
reproductions of public, distinctive code. It does **not** reliably find paraphrased
reproductions, code from sources that are not publicly indexed, or short idiomatic fragments that
are reproductions in fact but unremarkable in form. The pass therefore closes the **declared** case
and reduces the undeclared one; it does not eliminate it, and no statement in this set should be
read as claiming the repository is provably free of undeclared reproduction.

## Acceptance criteria

| # | Check | Proves |
|---|---|---|
| G1 | The notice file lists every dependency in the inbound register with its version and licence, and matches the versions recorded by Integrations | `POLICY-OUTBOUND-MIT`, the register |
| G2 | Every `DEP-###` has a register row, and no row reads **Open**, before any dependency is added to a build | `POLICY-CLEARANCE-GATES-SELECTION`, the population rule and the two-state vocabulary |
| G3 | No release artefact contains a SimHub assembly, verified by inspecting the artefact contents | `POLICY-NO-REDISTRIBUTION-SIMHUB` |
| G4 | Every non-trivial unit identified as adapted carries a `SOURCE:` marker, and every marker has a matching register entry with a determination — checked in both directions, since a marker without an entry and an entry without a marker are different failures | `POLICY-PROVENANCE-ATTESTATION` |
| G5 | A detection pass is recorded before the first public push, and before each release that added code, each carrying the residual statement | `POLICY-PROVENANCE-DETECTION` |
| G6 | A deliberately planted unit copied from a known copyleft source is caught by the pass, removed and reimplemented — exercising the defect path rather than assuming it works | `POLICY-INCOMPATIBLE-IS-DEFECT`, `POLICY-NO-COPYLEFT-SOURCE` |
| G7 | No prebuilt firmware binary appears in any release artefact while the source-only determination stands | the LGPL determination |

