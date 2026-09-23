# CLAUDE.md

Guidance for Claude Code working in the **cyd-sim-dash** repo.

## What this repo is

<!-- Fill in: one paragraph on what this product is and who it is for. -->
A four-element sim-racing dashboard on an ESP32 Cheap Yellow Display (ESP32-2432S028R,
320×240 ILI9341), fed over WiFi by a purpose-built SimHub plugin on the sim PC. Centre gear
indicator, background shift ramp/flash, and left/right edge bars for cars alongside — all
rendering at once. Two first-party components (C# SimHub plugin · ESP32 firmware) joined by a
versioned JSON-over-UDP wire contract, with **device-initiated addressing**: the device holds the
PC's address and registers itself; the plugin holds no configuration.

The doc set lives in **`docs/`** — see [`docs/README.md`](docs/README.md) for the index and
[`docs/manifest.yaml`](docs/manifest.yaml) for the authoritative scope and rungs.

## Documentation standard (Dictum)

This repo documents itself against the **Dictum** standard for build-ready
software documentation, installed here as a **version-pinned, vendored copy**.

**Installed version:** `v1.2.0` — the signed release tag from the canonical
repository (`git@github.com:pnjacket/dictum.git`). Verified at install time:
good GPG signature on the tag, and every vendored normative file byte-matches
the tag's blobs. Record this tag as `authored_against:` in the doc-set manifest
when `doc-scaffold` creates it; it is the start line for any later upgrade walk.

### Path resolution

The skills and agents below reference the standard's files **by bare name**.
Those bare references resolve under **`dictum/`** at this repo's root:

| Bare reference | Resolves to |
| --- | --- |
| `STANDARD.md` | `dictum/STANDARD.md` — the standard, Parts 0–13 |
| `GLOSSARY.md` | `dictum/GLOSSARY.md` |
| `failure-mode-catalog.md` | `dictum/failure-mode-catalog.md` |
| `EDITORIAL.md` | `dictum/EDITORIAL.md` |
| `concerns/11.x-*.md` | `dictum/concerns/` — the 15 concern specifications |
| `templates/` | `dictum/templates/` — concern-doc, single-file, manifest, binding-map, build-status |

Do **not** look for these at the repo root; `dictum/` is the only copy.

### Installed tooling

`.claude/skills/` — `doc-scaffold` (greenfield start), `doc-excavate`
(brownfield code→doc bootstrap), `doc-levelup`, `doc-feature` (doc-led forward
flow), `doc-change-impact`, `report-failure-mode`.

`.claude/agents/` — `doc-maturity-auditor`, `code-cartographer`,
`drift-detector`, `implementation-planner`, `concern-specialist`.

The tooling is **advisory**: it assists authoring and never defines
conformance. Every rule is re-derivable from `dictum/STANDARD.md`.

`install-dictum` is deliberately **not** installed here — it belongs to the
Dictum checkout. Upgrades are run from there, against this repo as the target.

### Core model (don't re-derive)

- **Two axes:** BREADTH (which of the 15 concerns apply, from product traits) ×
  DEPTH (5 rungs: `Absent → Sketch → Specified → Contract-grade │ Verified`).
- **Scope is the only calibration lever.** Contract-grade never weakens; a
  smaller product scopes more *out*, recorded in each doc's *Non-goals*.
- **Owned once, referenced everywhere** — every contract lives in one concern
  and is referenced by stable ID (`CAP-### · ENTITY-### · COMPONENT-### ·
  API-### · ROLE-### · SCREEN-### · ENV-### · CONFIG-### · OUT-###`).
- **Markers:** subject markers (`[GAP] [ASSUMPTION] [REVISIT] [FUTURE-SCOPE]`)
  stay in published docs; build markers (`<!-- BUILD: ... -->`) strip on publish.

### Next step

The doc set is scaffolded (2026-09-21); all ten in-scope concerns sit at `sketch`. Run
**`doc-levelup`** on **Product & Requirements** first — it mints the `CAP-###` register that every
other concern traces back to.

**v1 is iRacing-only** (decided 2026-09-21); AC/ACC and ETS2 are v2. iRacing is the only title
where all four elements work, so it is the only one that tests the concept rather than a subset.

**Titles are source modules, not a monolith.** Each sim is one module implementing a common
title-adapter interface, registered in one place, shipped in a single plugin assembly. The adapter
resolves everything sim-specific — including the shift-light fallback chain — and emits a
normalised frame with absolute RPM thresholds, so **the firmware contains no per-title logic** and
adding a title never means reflashing the panel.

Top remaining unknown: the **iRacing mapping table** — spotter property, whether per-car
shift-light values are usable, and the game-identity property that selects an adapter. All three
are answered in one sitting with SimHub's property picker.

## Licensing of the vendored material

`dictum/` is a verbatim copy of Dictum v1.2.0, **unmodified**. The standard's
prose (`STANDARD.md`, `GLOSSARY.md`, `failure-mode-catalog.md`, `EDITORIAL.md`,
`concerns/`) is **CC BY 4.0**, © 2026 David H. Jung and the Dictum
contributors. `templates/` and the installed `.claude/` skills and agents are
**MIT**. See `dictum/LICENSE` for the authoritative file→license map and
`dictum/LICENSES/` for the full texts. These licenses cover the *vendored
material only* — they say nothing about this product's own code or docs.
