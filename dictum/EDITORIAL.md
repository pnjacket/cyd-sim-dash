---
artifact: documentation-standard
role: editorial-charter
status: published
version: 1.1.0
audience: [contributor, maintainer, ai-assistant]
---

# The Editorial Charter

> The rules for **how the Dictum standard's own text is authored and maintained** — distinct from what the standard *says*. Every contributor (human or AI) follows these; they change only through the versioned process in Part 5.
>
> This charter is **self-exemplifying**: it obeys the rules it states — rule + one-line why, no war-stories, describe-by-shape.

## Part 0 — Purpose & scope

`CONTRIBUTING.md` governs **licensing and process** (which license, sign-off, open-an-issue-first). This charter governs **editorial form**: what belongs in the standard versus its companions, and how the prose is kept lean. Where a working convention in `CLAUDE.md` (this repo or `dictum-lab`) touches editorial form, this charter is the source and those files defer to it.

It binds anyone editing the normative prose — `STANDARD.md`, `concerns/`, `GLOSSARY.md`, `failure-mode-catalog.md` — and the maintainer accepting the change.

## Part 1 — The three-tier content split (the core rule)

Every piece of writing has exactly one home, decided by what it *is*:

| Tier | Home | Holds | The test |
|---|---|---|---|
| **Standard** | `STANDARD.md`, `concerns/`, `GLOSSARY.md` | The **rule**, plus at most **one distilled clause of why**. | Is this a rule an implementer or author must follow? |
| **Catalog** | `failure-mode-catalog.md` | The **distilled rationale** — each rule tied to the real failure it prevents. | Is this *why the rule exists*, abstracted from any one trial? |
| **Lab** | `dictum-lab` (studies, `concern-notes/`, fixtures) | The **evidence** — worked examples, trial narratives, measured numbers, war-stories. | Is this *what happened in a specific trial*? |

Three rules keep the tiers clean:

- **Rule + one-line why.** Normative prose states the rule and, at most, a single clause of rationale. Deeper "why" goes to the catalog; the trial that surfaced it goes to the lab.
- **No war-stories in normative text.** Phrases like "surfaced building a messaging SaaS", "a validation build stalled on…", "verified in practice", "422'd every id route" are evidence — they belong in the lab, never in a bar or a concern spec.
- **No research-process artifacts.** Internal development references — "(round 22)", "the v0.11 bar", "round-6 decision" — are deleted outright, never shipped.

## Part 2 — The standard stands alone

A reader must be able to follow **every rule** from the standard (and its in-repo catalog) with **no access to the lab**. Conversely, the standard **never depends on** the lab: no bar, gate, or procedure links to a lab tool or document. Back-pointers from the standard to lab content — when they are added — are **non-normative** and cite a stable **lab anchor** (`LAB-*`, registered in `dictum-lab/ANCHORS.md`), **pinned per release by the matching signed lab tag**: Standard `vX` cites the lab at the identically-versioned lab tag, cut together (`RELEASES.md`, *Lab citation pinning*). Only the cited narrative content rides the standard's version this way; the lab's tool-development cadence stays independent.

## Part 3 — Describe by shape, never by name

No committed file (standard or lab) may name a real product, repository, company, or the finance/accounting/leasing domain the work originated in. Products are described **by shape** ("a planning SaaS", "a client-only browser game"). This is a **legal constraint**, not a style preference; a name-scan gates publishing.

## Part 4 — Self-exemplifying

The standard obeys its own document contract. Match the existing front-matter and section structure; **reference contracts by ID, never duplicate** one another concern owns; and mint the standard's own IDs on a single **register line** in the pinned **ID grammar** (STANDARD Part 5). If you change a rule, keep `failure-mode-catalog.md` and the relevant `concerns/11.x` ⇄ `STANDARD.md` Part 11 in sync.

## Part 5 — How these rules change (rides the standard's version)

This charter carries a `version:` that **tracks the standard's** and changes through `RELEASES.md`'s policy:

- Most editorial-rule changes are **PATCH** (editorial/clarifying) — bump and note.
- A change that **forces re-sweeping existing text** (e.g. a new prohibited-phrase class, a new tier boundary) ships a **re-conformance note** in the release naming the sweep it asks for — the same discipline a MINOR standard release uses to keep conforming doc sets from silently stranding (failure-modes #32/#33). **No editorial rule changes silently.**

The charter is covered by the release's `SHA256SUMS` manifest alongside the other normative prose.

## Part 6 — Enforcement

- **Human gate (now):** the contributor checklist (Appendix B), surfaced in the PR template; the maintainer verifies it before merge.
- **Machine gate (lab-side): the editorial linter** (`dictum-lab`, `tools/editorial-lint/`). It checks the mechanically decidable subset: a **`standard → lab` dependency link** and a **forbidden-name** hit (ERROR, exit-driving); **research-prose smells** and a migrated **`## Worked Example` / `## Design Decisions`** heading in a concern spec (WARN, never exit-driving). Per Part 2 it lives in the lab and runs as **repo-hygiene CI** on this repo (`.github/workflows/editorial-lint.yml`) — deliberately *not* a conformance gate, so a lab-hosted tool checking the standard's prose does not make conformance depend on the lab. Register-form / ID-grammar are checked for *product doc sets* by the lab's `gate-check`, not here. The forbidden-name denylist is **local-only** (never committed; `.gitignore`). "Gates check artifacts, never attention" (STANDARD Part 0.7).

## Appendix A — The compaction procedure

To compact a concern spec (or apply the split to any normative doc):

1. **Keep the spine:** front-matter, the Purpose & Scope *boundary*, the Sub-aspects table, Owned Contracts (ID schemes + contract kinds), the Maturity Ladder, the Contract-grade bar.
2. **Migrate to a lab note** (`dictum-lab/concern-notes/11.x-<slug>.md`): the whole `## Worked Example` and `## Design Decisions` sections, verbatim.
3. **Before migrating a Design Decision, check whether it is the *sole* statement of a rule.** If so, **lift the rule** (distilled, rule + one-line why) into the normative spine first — never lose a rule.
4. **Strip embedded war-stories** from the spine; **de-duplicate** a rule stated 3–4× to a single statement (the bar references it tersely rather than re-deriving it).
5. **Self-check:** owned-contract ID set identical before/after · no bar rule dropped · Sub-aspects and Maturity-Ladder tables byte-identical · no meaning changed, no rule invented · no dangling `DD#N` / "see below" references left behind.

## Appendix B — Contributor PR checklist

- [ ] **Tier placement** — every addition is a rule (→ standard), rationale (→ catalog), or evidence (→ lab), in its correct home.
- [ ] **Rule + one-line why** — no normative prose carries more than one clause of rationale; no war-stories; no "(round N)" / "vX.Y bar" artifacts.
- [ ] **Stands alone** — no new `standard → lab` dependency; the rule is followable without the lab.
- [ ] **Describe by shape** — no real product/company/domain names anywhere in the diff.
- [ ] **Self-exemplifying** — front-matter and structure match; IDs referenced not duplicated; a new owned ID sits on one register line in the Part 5 grammar.
- [ ] **In sync** — a changed rule updates `failure-mode-catalog.md` and keeps `concerns/11.x` ⇄ `STANDARD.md` Part 11 consistent.
- [ ] **Version** — if an editorial rule itself changed, `EDITORIAL.md`'s version is bumped and `RELEASES.md` notes it (with a re-conformance step if a re-sweep is owed).
