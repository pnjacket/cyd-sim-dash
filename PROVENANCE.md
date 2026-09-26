# First-party source-provenance register

Required by `POLICY-PROVENANCE-ATTESTATION` and `POLICY-PROVENANCE-DETECTION` in
[`docs/governance-and-compliance.md`](docs/governance-and-compliance.md).

This repository is **model-authored**, which the standard treats as meaning provenance is
*undeclared by default*: a model can reproduce licensed source with no attribution and nobody
involved need realise. Attestation alone would reproduce exactly the blindness it exists to close,
so a detection pass is owed as well, and its residual recorded rather than read as "cleared".

Created with the first source file rather than reconstructed later — a register begun late silently
omits everything written before it.

## Attested units

Every non-trivial first-party unit copied, ported or adapted from an external source carries a
`SOURCE: <origin> <licence>` marker at its code site, in the host language's comment syntax, and an
entry here.

| Unit | Origin | Licence | Compatible with MIT outbound | Date |
|---|---|---|---|---|
| `cyd::touch::readPressure` — the XPT2046 pressure read | `XPT2046_Touchscreen` by Paul Stoffregen | MIT | Yes | 2026-09-26 |

**On that one entry.** The control bytes are re-derived from the datasheet's channel encoding, and the
pin assignment is a fact about the board. What is adapted is the part that is a *choice*: reading Z1
and Z2 and combining them as `z1 + 4095 - z2` rather than thresholding either alone, the
command-pipelining idiom where each transfer carries the next request, and the pressure floor of 400.
Those came from that library, and it is attested rather than described as hand-rolled because
"hand-rolled" and "written from nothing" are not the same claim — which is the whole point of this
register. The library itself is deliberately **not** a dependency: adding it would make a fourth
external library and break `R9` and `ADR-STD-LIBS`, and the read is three transactions.

**Where the detection pass looks.** Targeted rather than exhaustive, at the units most likely to be
reproductions. In this product that list is short and predictable:

- TFT_eSPI display initialisation and **pin configuration for the CYD** — by far the highest risk,
  since nearly every working CYD project derives its display setup from a small number of
  circulating examples, many carrying no licence header at all.
- Captive-portal handling.
- Any JSON parsing helpers written by hand rather than delegated to ArduinoJson.

## Detection-pass log

Runs before the first public push, and before each release that added code.

| Date | Scope | Findings | Residual |
|---|---|---|---|
| 2026-09-26 | The touch read added in slice 24, against the highest-risk list above | One adaptation, attested above. The display setup was reviewed at the same time and its pin block is a board fact rather than a reproduction; the driver/inversion combination was settled by observation on real glass, recorded in `tft_config.h` | The captive-portal handling and the hand-written JSON helpers have still not had a pass. Not cleared — owed |

## Residual statement

Reproduced at each pass, and not to be softened.

The detection pass finds recognisable reproductions of **public, distinctive** code. It does **not**
reliably find paraphrased reproductions, code from sources that are not publicly indexed, or short
idiomatic fragments that are reproductions in fact but unremarkable in form.

The pass therefore closes the **declared** case and reduces the undeclared one. It does not
eliminate it, and no statement in this repository should be read as claiming the source is provably
free of undeclared reproduction.

A green dependency-licence scan is **not** source-provenance clearance: it is structurally blind to
code copied into first-party source, having no package and no import edge to follow.
