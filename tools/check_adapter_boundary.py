#!/usr/bin/env python3
"""Check R2 - the adapter boundary holds.

Architecture's check R2: "Searching COMPONENT-PLUGIN-CORE for any sim name or sim-specific
property finds nothing; all such references live in an adapter."

That is the contract that makes ADR-ADAPTER-MODULES real rather than aspirational. Without an
automated check it degrades quietly: someone reaches for one iRacing property in the core to fix
one bug, and the boundary that was supposed to make a v2 title cheap is gone with no failing test
to say so.

Comments are exempt. The boundary is about what the code DOES; a comment explaining why the
boundary exists is not a breach of it, and forbidding the explanation would be perverse.

Usage:  python tools/check_adapter_boundary.py
Exit:   0 if the boundary holds, 1 otherwise.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE = ROOT / "plugin" / "src" / "core"
BRIDGE = ROOT / "plugin" / "src" / "simhub"

# The registry is the composition root and is EXPECTED to name titles - that is its whole job,
# and "registered in one place" is the design. It is the one file exempt from the sweep.
EXEMPT = {"AdapterRegistry.cs"}

# Sim names and sim-specific property fragments. Deliberately broad: a false positive costs one
# exemption, a false negative costs the architecture.
FORBIDDEN = [
    r"\biracing\b",
    r"\bassetto\b",
    r"\bacc\b",
    r"\beuro\s*truck\b",
    r"\bets2\b",
    r"\brfactor\b",
    r"\bautomobilista\b",
    r"\bdirt\s*rally\b",
    r"DataCorePlugin",
    r"GameRawData",
    r"DriverCarSL",
    r"CarLeftRight",
    r"SpotterCar",
    r"CarSettings_",
]


def strip_comments(text: str) -> list[tuple[int, str]]:
    """Return (line number, code) with // and /* */ comment bodies blanked out.

    Crude but sufficient: this is a boundary sweep, not a C# parser. Blanking rather than dropping
    keeps the line numbers honest so a breach reports where it actually is.
    """
    out: list[tuple[int, str]] = []
    in_block = False
    for n, line in enumerate(text.splitlines(), 1):
        code = ""
        i = 0
        while i < len(line):
            if in_block:
                end = line.find("*/", i)
                if end == -1:
                    break
                in_block = False
                i = end + 2
                continue
            if line.startswith("//", i):
                break
            if line.startswith("/*", i):
                in_block = True
                i += 2
                continue
            code += line[i]
            i += 1
        out.append((n, code))
    return out


def main() -> int:
    if not CORE.is_dir():
        sys.exit(f"no plugin core at {CORE}")

    patterns = [(p, re.compile(p, re.IGNORECASE)) for p in FORBIDDEN]
    breaches: list[str] = []
    scanned = 0

    for directory in (CORE, BRIDGE):
        if not directory.is_dir():
            continue
        for path in sorted(directory.rglob("*.cs")):
            # Adapters are exactly where sim-specific knowledge belongs.
            if "Adapters" in path.parts or path.name in EXEMPT:
                continue
            scanned += 1
            for lineno, code in strip_comments(path.read_text(encoding="utf-8")):
                for label, rx in patterns:
                    if rx.search(code):
                        rel = path.relative_to(ROOT).as_posix()
                        breaches.append(f"  {rel}:{lineno}  matches /{label}/\n      {code.strip()}")

    if breaches:
        print("FAIL  R2 - sim-specific references found outside the adapters:")
        print("\n".join(breaches))
        print("\nMove them into an adapter, or into AdapterRegistry if this is registration.")
        return 1

    print(f"PASS  R2 - adapter boundary holds across {scanned} core and bridge files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
