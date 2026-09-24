#!/usr/bin/env python3
"""Fail if any tracked text file contains a stray control character.

This exists because one did, and it cost every CI run on the repository. A path in the workflow
read `Framework64\v4.0.30319`, and an editing pass turned that `\v` into an actual vertical tab.
GitHub then rejected the whole workflow file as unparseable - every job, with no log and a zero
second duration, which looks nothing like "one character is wrong".

The same class has bitten three times here: a `\b` produced `pluginuild` in a build script, a `\0`
became a literal NUL in two C++ sources, and this. They share a shape: an editing tool interpreted
an escape that was meant to stay literal, and the result is invisible in every normal view of the
file.

Usage:  python tools/check_no_control_chars.py
Exit:   0 if clean, 1 otherwise.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# Tab, newline and carriage return are the legitimate ones.
ALLOWED = {0x09, 0x0A, 0x0D}

BINARY_SUFFIXES = {
    ".png", ".jpg", ".jpeg", ".gif", ".ico", ".bin", ".elf", ".dll", ".exe",
    ".zip", ".gz", ".pdf", ".woff", ".woff2", ".ttf",
}


def tracked_files() -> list[Path]:
    out = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True)
    return [Path(line) for line in out.stdout.splitlines() if line.strip()]


def main() -> int:
    bad: list[str] = []
    checked = 0

    for path in tracked_files():
        if path.suffix.lower() in BINARY_SUFFIXES or not path.is_file():
            continue
        data = path.read_bytes()
        if b"\x00" in data[:8192] and path.suffix.lower() not in {".py", ".cs", ".cpp", ".h", ".ino"}:
            continue  # genuinely binary, unflagged suffix
        checked += 1
        for offset, byte in enumerate(data):
            if byte < 0x20 and byte not in ALLOWED:
                line = data[:offset].count(b"\n") + 1
                bad.append(f"  {path}:{line}  contains 0x{byte:02x}")
                break

    if bad:
        print("FAIL  stray control characters in tracked text files:")
        print("\n".join(bad))
        print("\nThese survive editing invisibly and break parsers in ways that do not name them.")
        return 1

    print(f"PASS  no stray control characters across {checked} tracked text files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
