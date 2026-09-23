#!/usr/bin/env python3
"""Contract tier: validate every shared fixture against the wire schemas.

This is the check that stops two independently-updated halves, in two languages, from drifting
apart silently. It runs in CI on every push.

The invalid fixtures matter as much as the valid ones. Six rows of the error catalogue name the
replay tool as their forcing mechanism, and these are the payloads it sends — a build that accepts
one of them has broken a contract just as surely as one that rejects a valid frame.

Usage:  python tools/check_fixtures.py
Exit:   0 if every fixture behaves as its directory claims, 1 otherwise.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    from jsonschema import Draft202012Validator
except ImportError:  # pragma: no cover
    sys.exit("jsonschema is required: python -m pip install jsonschema")

ROOT = Path(__file__).resolve().parent.parent
CONTRACTS = ROOT / "contracts"
FIXTURES = CONTRACTS / "fixtures"


def load(path: Path) -> dict:
    with path.open(encoding="utf-8") as fh:
        return json.load(fh)


def schema_for(fixture_name: str) -> str:
    """Fixtures are routed by filename prefix; registration fixtures say so in their name."""
    return "registration" if fixture_name.startswith("registration") else "frame"


def relational_errors(doc: dict, kind: str) -> list[str]:
    """Invariants JSON Schema cannot express.

    A JSON Schema describes the *shape* of a document; it cannot compare two fields to each other.
    INV-RAMP-ORDER is exactly that kind of rule, so it lives here. Every implementation must
    enforce both layers — the schema alone is not the contract.
    """
    errors: list[str] = []
    if kind != "frame":
        return errors

    ramp, flash = doc.get("rampStartRpm"), doc.get("flashRpm")
    if ramp is not None and flash is not None and not ramp < flash:
        errors.append(
            f"INV-RAMP-ORDER: rampStartRpm ({ramp}) must be strictly less than flashRpm ({flash})"
        )
    if (ramp is None) != (flash is None):
        errors.append("INV-RAMP-ORDER: the threshold pair must be present or absent together")

    return errors


def main() -> int:
    validators = {
        name: Draft202012Validator(load(CONTRACTS / f"{name}.schema.json"))
        for name in ("frame", "registration")
    }

    failures: list[str] = []
    counts = {"valid": 0, "invalid": 0}

    for expected_valid, folder in ((True, "valid"), (False, "invalid")):
        for path in sorted((FIXTURES / folder).glob("*.json")):
            kind = schema_for(path.stem)
            doc = load(path)
            messages = [e.message for e in validators[kind].iter_errors(doc)]
            messages += relational_errors(doc, kind)
            actually_valid = not messages
            counts[folder] += 1

            if actually_valid != expected_valid:
                if expected_valid:
                    detail = "; ".join(messages)
                    failures.append(f"{folder}/{path.name}: should validate, but {detail}")
                else:
                    failures.append(
                        f"{folder}/{path.name}: should have been rejected, but it passed both layers"
                    )

    total = counts["valid"] + counts["invalid"]
    print(f"{total} fixtures checked  ({counts['valid']} valid, {counts['invalid']} invalid)")

    if failures:
        print(f"\n{len(failures)} failure(s):")
        for f in failures:
            print(f"  {f}")
        return 1

    print("all fixtures behave as their directory claims")
    return 0


if __name__ == "__main__":
    sys.exit(main())
