#!/usr/bin/env python3
"""Validate the adapter's real emitted frames against the wire schema.

This is what makes `CONFORMANCE-ADAPTER` C4 mean something. The C# suite can only assert that a
key is present in a string; it has no JSON Schema validator and, on a machine with nothing but
the Framework compiler, cannot acquire one. So the suite emits its frames to a file and this runs
the same validator the shared fixtures run — including the relational rule that JSON Schema alone
cannot express.

Without this step C4 would be a spelling test. With it, the adapter is held to the identical
contract the device is held to, by the identical code.

Usage:  python tools/check_adapter_output.py <emitted.ndjson>
Exit:   0 if every emitted frame is contract-conformant, 1 otherwise.
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
SCHEMA = ROOT / "contracts" / "frame.schema.json"


def relational_errors(doc: dict) -> list[str]:
    """The rules JSON Schema cannot state, because it cannot compare two fields.

    Kept deliberately in step with tools/check_fixtures.py: if these two ever disagree, the
    adapter and the device are being held to different contracts, which is the exact failure the
    shared-fixture arrangement exists to prevent.
    """
    errs: list[str] = []

    ramp, flash = doc.get("rampStartRpm"), doc.get("flashRpm")
    if ramp is not None and flash is not None and not ramp < flash:
        errs.append(f"INV-RAMP-ORDER: rampStartRpm {ramp} is not below flashRpm {flash}")

    # One threshold without the other is not a usable pair; the chain emits both or neither.
    if (ramp is None) != (flash is None):
        errs.append("INV-RAMP-ORDER: exactly one of the threshold pair is present")

    for key in ("rpm", "rampStartRpm", "flashRpm"):
        v = doc.get(key)
        if v is not None and v < 0:
            errs.append(f"INV-RPM-NONNEG: {key} is negative ({v})")

    # A non-live frame must not carry telemetry: a stale value behind a fault status is worse
    # than no value, because the device cannot tell it is stale.
    if doc.get("status") != "live":
        for key in ("gear", "rpm", "rampStartRpm", "flashRpm", "spotterLeft", "spotterRight"):
            if doc.get(key) is not None:
                errs.append(f"INV-STATUS-CONSISTENT: status {doc['status']!r} carries {key}")

    return errs


def main() -> int:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    if not path.is_file():
        sys.exit(f"no such file: {path}")

    validator = Draft202012Validator(json.loads(SCHEMA.read_text(encoding="utf-8")))

    checked = failed = 0
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        checked += 1
        try:
            doc = json.loads(line)
        except json.JSONDecodeError as exc:
            print(f"  FAIL  line {lineno}: not valid JSON - {exc}")
            failed += 1
            continue

        problems = [e.message for e in validator.iter_errors(doc)]
        problems += relational_errors(doc)
        if problems:
            failed += 1
            print(f"  FAIL  line {lineno}: {doc.get('status')}")
            for p in problems:
                print(f"          {p}")

    if failed:
        print(f"\nFAIL  {failed} of {checked} emitted frames breach the wire contract")
        return 1
    print(f"PASS  {checked} emitted frames are contract-conformant")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
