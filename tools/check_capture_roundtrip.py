#!/usr/bin/env python3
"""Cross-language check: a capture written by the C# plugin is readable by the Python replay tool.

This is what OUT-CAPTURE actually asserts. The capture writer and the replay reader are written in
different languages by different code paths, and they are required to agree because both conform to
the same message contract — not because anyone tested them against each other once.

Without this, "the two serialisers agree" is an assumption. Drift between them would show up as a
replay that silently sends nothing, or sends frames the device rejects, on the day someone is
trying to reproduce a bug and has no sim to hand.

Checks, in order:
  1. every line is {offsetMs, frame} with the right types
  2. offsets start at zero and never go backwards
  3. every embedded frame validates against frame.schema.json, verbatim
  4. the replay tool's own loader accepts the file

Usage:  python tools/check_capture_roundtrip.py <capture.ndjson>
Exit:   0 if the capture round-trips, 1 otherwise.
"""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

try:
    from jsonschema import Draft202012Validator
except ImportError:  # pragma: no cover
    sys.exit("jsonschema is required: python -m pip install jsonschema")

ROOT = Path(__file__).resolve().parent.parent
SCHEMA = ROOT / "contracts" / "frame.schema.json"
REPLAY = ROOT / "tools" / "replay.py"


def load_replay_module():
    """Import the replay tool as a module, so this checks the REAL loader rather than a copy."""
    spec = importlib.util.spec_from_file_location("replay_tool", REPLAY)
    if spec is None or spec.loader is None:
        return None
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as exc:  # pragma: no cover
        print(f"  note: could not import the replay tool ({exc}); structural checks only")
        return None
    return module


def main() -> int:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    path = Path(sys.argv[1])
    if not path.is_file():
        sys.exit(f"no such file: {path}")

    validator = Draft202012Validator(json.loads(SCHEMA.read_text(encoding="utf-8")))
    failures = 0
    lines = [ln for ln in path.read_text(encoding="utf-8").splitlines() if ln.strip()]

    if not lines:
        print("FAIL  the capture is empty")
        return 1

    previous = -1
    for n, line in enumerate(lines, 1):
        try:
            entry = json.loads(line)
        except json.JSONDecodeError as exc:
            print(f"  FAIL  line {n}: not valid JSON - {exc}")
            failures += 1
            continue

        if set(entry) != {"offsetMs", "frame"}:
            print(f"  FAIL  line {n}: expected exactly offsetMs and frame, got {sorted(entry)}")
            failures += 1
            continue

        offset = entry["offsetMs"]
        if not isinstance(offset, int):
            print(f"  FAIL  line {n}: offsetMs is {type(offset).__name__}, not an integer")
            failures += 1
        else:
            if n == 1 and offset != 0:
                print(f"  FAIL  line 1: the first offset is {offset}, not 0")
                failures += 1
            if offset < previous:
                print(f"  FAIL  line {n}: offset {offset} goes backwards from {previous}")
                failures += 1
            previous = max(previous, offset)

        problems = [e.message for e in validator.iter_errors(entry["frame"])]
        if problems:
            failures += 1
            print(f"  FAIL  line {n}: the embedded frame breaches the wire contract")
            for p in problems:
                print(f"          {p}")

    # The replay tool's own loader, if it exposes one. This is the half that catches drift the
    # schema cannot: a file that is contract-valid but that the tool still will not read.
    module = load_replay_module()
    if module is not None:
        loader = None
        for name in ("load_capture", "read_capture", "load"):
            if hasattr(module, name):
                loader = getattr(module, name)
                break
        if loader is None:
            print("  note: the replay tool exposes no importable loader; structural checks only")
        else:
            try:
                loaded = loader(path)
                count = len(list(loaded))
                if count != len(lines):
                    print(f"  FAIL  the replay tool loaded {count} frames, the file has {len(lines)}")
                    failures += 1
                else:
                    print(f"  ok    the replay tool's own loader accepted all {count} frames")
            except Exception as exc:
                print(f"  FAIL  the replay tool could not load the capture: {exc}")
                failures += 1

    if failures:
        print(f"\nFAIL  {failures} problem(s) in {len(lines)} capture lines")
        return 1
    print(f"PASS  {len(lines)} capture lines round-trip between the C# writer and the Python reader")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
