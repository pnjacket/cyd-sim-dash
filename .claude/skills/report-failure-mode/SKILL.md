---
name: report-failure-mode
description: Use when a product built to the Dictum standard hits a failure the standard did NOT prevent — a build-ready or shipped defect that got past the concern bars, the gates, and the tooling — and you want to contribute it back. Distills the one failure into a structured, describe-by-shape failure-mode report (Observed failure · Root cause · Preventing mechanism · a draft catalog row) for the Dictum maintainer to adopt or decline. Writes the report to the product's scratch only; never edits the standard. Triggers - "report this failure to Dictum", "the standard didn't catch this — file it upstream", "contribute a failure mode", "draft a failure-mode report", "propose a new catalog entry".
---

# report-failure-mode

Operationalizes the **contribution path** (`CONTRIBUTING.md`, `EDITORIAL.md`): when a product built to Dictum hits a failure the standard's bars, gates, and tooling let through, this skill turns that one incident into a distilled **failure-mode report** the maintainer can weigh. It is the generator of a report shaped like the entries in `failure-mode-catalog.md` — one observed failure, one root cause, one preventing mechanism, one draft row.

Advisory, and deliberately **narrow**: it *proposes*, it never adopts. It writes a single markdown report into the **product repo's scratch** (not committed to the product, and never into the Dictum standard) — adopting the proposal is the maintainer's call, through the normal issue/PR process.

## Inputs you rely on
- The **Dictum standard** you built against: `STANDARD.md`, the implicated `concerns/11.x-*.md` (their sub-aspect keys + owned-contract kinds), `failure-mode-catalog.md` (the target row format and the density to match), `GLOSSARY.md`.
- The **contribution rules** the report must respect: `CONTRIBUTING.md` (three-tier split, inbound=outbound) and `EDITORIAL.md` (rule + one-line why · no war-stories · **describe by shape**, Part 3).
- The **incident**: what failed, what the standard-prescribed checks reported, and how it was actually caught — from the operator and the product repo.

## Procedure

1. **Confirm it's a standard gap, not a product gap.** The report is warranted only when a *conforming* set still let the failure through — the bars were met, the gates ran, the tooling didn't warn, and it broke anyway (or a bar was met *as written* and was wrong). If the real story is "a bar was skipped" or "the doc was never taken to Contract-grade", that's a product fix, not a contribution — say so and stop. One failure per report; if several surfaced, pick the one and run again for the next.

2. **Reconstruct the failure precisely, in product terms first.** Get the concrete story straight *before* abstracting: what happened, which standard-prescribed checks reported what (the concern bar that was met, the gate that passed green, the tool that stayed silent), and how it was *actually* caught (usually not by the method — a runtime break, an audit, a human noticing). This concrete version stays in the **product's own scratch** as the evidence; only its abstracted shape reaches the report.

3. **Locate the implicated concerns by number + sub-aspect key.** Name each concern the failure touches (`11.x`) and the specific published **sub-aspect key** (and owned-contract kind, e.g. `LEGAL-###`) whose bar was in play — the same keys the manifest partitions over. This is what makes the report actionable: it points the maintainer at exactly the bar(s) that would move.

4. **Decompose the root cause — *why the standard let it through*.** Not the proximate bug; the reason a conforming set didn't catch it. Push for the decomposition the catalog rewards, typically two strands:
   - a **scope/framing gap** — a bar carved along the wrong axis, a concern scoped to too narrow a boundary, a responsibility with no always-on home (a rule written against the *stack* that surfaced it rather than the *behavior* it binds);
   - a **model/assumption gap** — the method assumed something that doesn't hold (e.g. "declared and known" when the reality is undeclared/unknown), so the mechanism that would fire never had an input.
   Naming both is what separates a distilled entry from a bug report.

5. **Propose a preventing mechanism that respects Dictum's design invariants.** The proposal must land *inside* the standard's model, not fight it. Check it against every invariant — bake these in:
   - **Scope is the only calibration lever.** Right-size by scoping in/out (a trait, a new sub-aspect key, a Non-goal), never by inventing a rigor/intent axis.
   - **Owned once, referenced everywhere.** A new contract gets exactly one owning concern; others reference it by ID. Don't duplicate.
   - **Contract-grade never weakens.** A fix narrows or widens *scope*; it never lowers the bar. If your fix reads as "require less", it's wrong.
   - **Doc maturity ≠ implementation status.** Never couple a doc-status/lifecycle signal to build evidence; keep the two orthogonal (detection of a build fact lives with build evidence, not with doc maturity).
   - **Prefer additive over rename.** A **new sub-aspect key / trait / contract kind** (additive, MINOR) is strongly preferred to renaming or re-partitioning existing vocabulary (a rename is MAJOR and strands conforming sets). Reach for "add a key" before "change a key".
   - **Tooling is advisory; the standard defines extension points, not tools.** If detection is part of the fix, the *normative* half is a marker/extension point (an in-code token like `DICT: <ID>` / `SOURCE: <origin>`) or a named auditor check; the **detector itself belongs in the research companion (`dictum-lab`)**, never in the standard. Model-A (judgment) vs model-B (deterministic) belongs where each already lives.
   - **Name the honest limit, don't over-promise a gate.** If the mechanism is best-effort (an attestation that can be evaded, a detector that can miss), say so as a Part 0.7 honest limit — recorded, never read as a guarantee. "Gates check artifacts, never attention."
   Where the fix touches a concern spec, note the `11.x` home and the sub-aspect it extends — as a *suggestion* for the maintainer, phrased so it slots into the existing bar without duplicating an owned contract.

6. **Scrub to describe-by-shape (the load-bearing step — do this before the report leaves the product repo).** The report is destined for a public standard; a concrete report is exactly where a regulated/sensitive **domain leaks in**. Walk the whole draft against the checklist below and rewrite every hit to its abstract shape, moving the concrete specifics into the product's own scratch (not the report). The rule is `EDITORIAL.md` Part 3 — this is a **legal constraint, not a style choice**, and a name-scan gates publishing.
   - [ ] **No product / repo / company / person name** — describe the product *by shape* ("an MIT-licensed, LLM-built library", "a real-time multi-tenant messaging SaaS", "a client-only browser game"), never its name.
   - [ ] **No sensitive/regulated-domain signal** — strip every finance / accounting / leasing / health / legal-domain term, and any **domain-specific function, entity, endpoint, or field name** that would reveal the domain (a named financial function in the evidence leaks the domain into the public standard — this is the exact leak the scrub exists to stop). Keep the failure→mechanism abstract; the concrete names stay in scratch.
   - [ ] **No war-stories in the mechanism text** — the *proposed* mechanism reads as a rule + one-line why (it's a draft for normative prose); the trial narrative stays in Observed failure and, in full, in scratch.
   - [ ] **No research-process artifacts** — no "(round N)", no "vX.Y bar", no internal ticket IDs.
   - [ ] **Standard stands alone** — the report proposes no dependency from the standard onto the lab; a detector is referenced as living *in* the lab, not linked *from* a bar.
   State in the report that the scrub was performed and that concrete specifics were retained only in the product's scratch.

7. **Write the report to the product's scratch.** One markdown file (e.g. `scratch/dictum-failure-report-<slug>.md` in the product repo — never under a vendored `dictum/`, never a commit to the standard). Structure, mirroring a catalog entry:
   - **Header** — *Reported-from* (product **shape**, never name) · *Standard version in use* (the set's `authored_against:` tag) · *Concerns implicated* (`11.x` + sub-aspect key(s), + owned-contract kind if any) · *Severity* + one line on why.
   - **Observed failure** — what happened · what the standard-prescribed checks reported (the bar met, the gate green, the tool silent) · how it was *actually* caught.
   - **Root cause** — *why the standard let it through*, decomposed (scope/framing gap · model/assumption gap).
   - **Preventing mechanism (proposed)** — concrete, invariant-respecting (step 5), with the `11.x` home(s) named as a suggestion; the honest limit stated.
   - **Suggested failure-mode-catalog entry** — a single row in the **exact** format: `| # | Observed failure | Root cause | Preventing mechanism (where) |`, leaving `#` as `N` for the maintainer to number. Match the catalog's density — abstract, one failure, tied to where the mechanism lands; note the sibling entry if one is obvious.

8. **Report and hand off.** Tell the operator the report path, the one-line failure it distills, the concerns implicated, and that **nothing was committed and nothing in the standard was edited**. Recommend the contribution route: open an issue on the Dictum repo first (per `CONTRIBUTING.md` — a rule change should name the failure it prevents), attaching the scrubbed report; adopting it (editing `STANDARD.md` / `concerns/` / `failure-mode-catalog.md`) is the **maintainer's** call.

## Guardrails
- **Report, don't self-adopt.** This skill writes a proposal to the product's scratch. It **never** edits `STANDARD.md`, `concerns/`, `failure-mode-catalog.md`, or any standard prose — adoption is the maintainer's, through issue/PR.
- **Describe by shape, always.** No product / repo / company / person name and **no sensitive-domain signal** (incl. domain-specific function/entity/field names) may reach the report — that scrub (step 6) is the point of this skill; concrete specifics live only in the product's scratch. A name-scan gates publishing (`EDITORIAL.md` Part 3).
- **One failure, distilled.** Match the catalog's density — a real observed failure, a decomposed root cause, a mechanism tied to where it lands. Several failures → several runs, one report each.
- **Respect the invariants (step 5).** A proposal that weakens Contract-grade, invents a third axis, duplicates an owned contract, couples doc-status to build evidence, puts a detector in the standard, or renames where a key would add — is wrong by construction. Prefer additive; name the honest limit.
- **It must be a standard gap.** If a bar was skipped or a doc never reached Contract-grade, that's a product fix, not a contribution (step 1) — don't manufacture an upstream report from a local process miss.
