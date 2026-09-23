#!/usr/bin/env python3
"""Replay tool — drives a device with no SimHub, no sim and no rig.

Realises COMPONENT-REPLAY. It substitutes the *entire PC chain* — SimHub, the plugin, the adapter
and the sim — at the wire contract. That is a legitimate substitution under E2E-STANDARD rather
than a bypass: every line of firmware still runs for real.

It has two jobs, and the second is why it exists this early:

  replay   send a captured lap, for realism and regression
  synth    generate frames that no real adapter would ever produce, for everything a lap
           cannot contain — malformed payloads, inverted thresholds, transposed stamps,
           bumped majors, every gear, both bars at once

**Synthetic mode must work with no captured lap present.** Six rows of the error catalogue name
this tool as their forcing mechanism, and if it required a real capture then every firmware slice
would stall behind rig availability. That is an acceptance condition, not a convenience.

Usage
-----
  python tools/replay.py synth --host 192.168.1.50 --scenario sweep
  python tools/replay.py synth --host cyd-sim-dash.local --scenario sweep --loop
  python tools/replay.py synth --host 192.168.1.50 --scenario malformed
  python tools/replay.py replay captures/lap.ndjson --host 192.168.1.50
  python tools/replay.py listen                      # act as the PC side, answer registrations

The device registers to us; we reply to the source address, exactly as the plugin does. With
--host you may also push blind to a known address, which is what the scenarios below do by default.
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from pathlib import Path

PORT = 47110  # fixed by contract; the plugin holds no configuration
PROTOCOL_MAJOR = 1
PROTOCOL_MINOR = 0

ROOT = Path(__file__).resolve().parent.parent


# ---------------------------------------------------------------------------
# Frame construction
# ---------------------------------------------------------------------------

def frame(stamp: int, *, status: str = "live", title: str | None = "iracing",
          gear: str | None = "4", rpm: float | None = 7200.0,
          ramp: float | None = 7800.0, flash: float | None = 8400.0,
          left: str | None = "none", right: str | None = "none",
          major: int = PROTOCOL_MAJOR, minor: int = PROTOCOL_MINOR) -> dict:
    return {
        "protocolMajor": major,
        "protocolMinor": minor,
        "status": status,
        "titleId": title,
        "stamp": stamp,
        "gear": gear,
        "rpm": rpm,
        "rampStartRpm": ramp,
        "flashRpm": flash,
        "spotterLeft": left,
        "spotterRight": right,
    }


def idle(stamp: int, status: str, title: str | None = None) -> dict:
    """A non-live frame. Every telemetry element null, per INV-STATUS-CONSISTENT."""
    return frame(stamp, status=status, title=title, gear=None, rpm=None,
                 ramp=None, flash=None, left=None, right=None)


# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------

def scenario_sweep(stamp: int):
    """A slow RPM climb, a long hold above the flash point, then back down. U4, U5, A3.

    Paced for a human to judge rather than for a machine to assert. The earlier version swept the
    whole range in about a second and a half and spent barely a moment above the flash point, which
    is useless for deciding whether a blink rate feels right - by the time you have focused on it,
    it has gone.

    The climb takes about five seconds so each ramp stage can be seen arriving, and the hold above
    the flash point runs for eight, which is roughly twenty-four blinks: long enough to form an
    opinion about the rate rather than to catch a glimpse of it.
    """
    # Below the ramp: black, so the first stage arriving is unmistakable.
    for _ in range(60):                                  # ~1 s
        yield frame(stamp, rpm=7400.0)
        stamp += 16

    # The ramp window, 7800 -> 8400, climbed slowly enough to see each stage land.
    for rpm in range(7800, 8400, 2):                     # 300 frames, ~5 s
        yield frame(stamp, rpm=float(rpm))
        stamp += 16

    # Held above the flash point. This is the part that exists to be judged.
    for _ in range(480):                                 # ~8 s, about 24 blinks
        yield frame(stamp, rpm=8700.0)
        stamp += 16

    # Back down through the stages, quickly - the descent is not what is being assessed.
    for rpm in range(8400, 7400, -20):
        yield frame(stamp, rpm=float(rpm))
        stamp += 16


def scenario_gears(stamp: int):
    """Every gear in the domain, including the two-character truck cases. A2, U3.

    Each gear is held for about a second and a half. The earlier pacing gave each one a fifth of a
    second, which is fine for asserting that a value arrived and useless for the question this
    scenario exists to answer: whether the glyph fits the region without clipping. A two-character
    gear drops to a smaller face, and noticing that it is cramped takes longer than a glance.

    RPM sits below the ramp throughout, so the bands stay black and nothing competes for attention.
    """
    for g in ["R", "N"] + [str(n) for n in range(1, 19)]:
        for _ in range(90):                 # ~1.5 s each, ~30 s for the full domain
            yield frame(stamp, gear=g, rpm=7000.0)
            stamp += 16


def scenario_spotter(stamp: int):
    """Every proximity combination, including both sides at once and the unavailable case. U7, X3."""
    combos = [("none", "none"), ("one", "none"), ("none", "one"), ("one", "one"),
              ("two", "none"), ("none", "two"), (None, None)]
    for left, right in combos:
        for _ in range(90):                 # ~1.5 s each, long enough to check which side is lit
            yield frame(stamp, left=left, right=right, rpm=7000.0)
            stamp += 16


def scenario_composition(stamp: int):
    """Every bar combination during a red flash - the case the composition rule exists for. U6, U7.

    Each combination is held for about two seconds. Holding rather than cycling quickly is the
    point: U6 asks whether a lit bar stays solid white through BOTH phases of the flash, and that
    is a question about watching one steady state, not about catching a transition.

    The both-sides row matters most. It cannot be staged on a real track - the operator reports
    getting a car on each side simultaneously is impractical - so this is the only place the
    rendering of that case can be judged at all.
    """
    combinations = [
        ("one", "none"),    # left only
        ("none", "one"),    # right only
        ("one", "one"),     # both - the case a real track will not produce on demand
        ("none", "none"),   # neither, so the flash is seen without bars for comparison
    ]
    for left, right in combinations:
        for _ in range(120):
            yield frame(stamp, rpm=8700.0, left=left, right=right)
            stamp += 16


def scenario_statuses(stamp: int):
    """Cycle live / noSim / unsupportedTitle / adapterFault. R8, U8."""
    for status, title in [("live", "iracing"), ("noSim", None),
                          ("unsupportedTitle", "Assetto Corsa"), ("adapterFault", "iracing")]:
        for _ in range(60):
            yield frame(stamp) if status == "live" else idle(stamp, status, title)
            stamp += 16


def scenario_out_of_order(stamp: int):
    """Transposed stamps, then a producer restart. Forces ERR-OUT-OF-ORDER; proves D2 and D3."""
    yield frame(stamp);      stamp += 16
    yield frame(stamp + 32)                      # ahead
    yield frame(stamp)                           # transposed - must be rejected
    yield frame(stamp + 48)
    yield frame(12)                              # backwards jump: a new producer run
    yield frame(28)


def scenario_version(stamp: int):
    """A bumped minor (accepted, unknown field ignored), then a bumped major (refused whole)."""
    f = frame(stamp, minor=PROTOCOL_MINOR + 3)
    f["someFutureField"] = 42
    yield f
    yield frame(stamp + 16, major=PROTOCOL_MAJOR + 1)


def scenario_field_range(stamp: int):
    """One crafted frame per invariant. Forces ERR-FIELD-RANGE."""
    yield frame(stamp, ramp=8400.0, flash=7800.0)          # inverted
    yield frame(stamp + 16, ramp=8000.0, flash=8000.0)     # equal
    yield frame(stamp + 32, rpm=-1.0)                      # negative
    yield frame(stamp + 48, gear="19")                     # out of domain
    bad = idle(stamp + 64, "noSim"); bad["gear"] = "4"     # non-live carrying telemetry
    yield bad


SCENARIOS = {
    "sweep": scenario_sweep,
    "gears": scenario_gears,
    "spotter": scenario_spotter,
    "composition": scenario_composition,
    "statuses": scenario_statuses,
    "out-of-order": scenario_out_of_order,
    "version": scenario_version,
    "field-range": scenario_field_range,
}

# Payloads that are not JSON at all, or not objects. Forces ERR-MALFORMED.
MALFORMED = [
    b"",
    b"{",
    b'{"protocolMajor": 1, "status": "live"',        # truncated
    b"not json at all",
    b"[1,2,3]",
    b'{"protocolMajor": "one"}',
    b"\x00\x01\x02\x03",
    b'{"protocolMajor":1,"rpm":' + b"9" * 900 + b"}",  # long digit string - SEC-PARSER-FLOOR
    b"x" * 3000,                                       # over the 2 KB cap - SEC-INPUT-BOUND
]


# ---------------------------------------------------------------------------
# Transport
# ---------------------------------------------------------------------------

def send_all(sock: socket.socket, target, payloads, rate_hz: float, label: str) -> int:
    interval = 1.0 / rate_hz if rate_hz > 0 else 0.0
    n = 0
    for p in payloads:
        data = p if isinstance(p, (bytes, bytearray)) else json.dumps(p).encode("utf-8")
        sock.sendto(data, target)
        n += 1
        if interval:
            time.sleep(interval)
    print(f"  {label}: sent {n} datagram(s) to {target[0]}:{target[1]}")
    return n


def cmd_synth(args) -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = (args.host, args.port)
    stamp = int(time.monotonic() * 1000) % 1_000_000

    if args.scenario == "malformed":
        return 0 if send_all(sock, target, MALFORMED, args.rate, "malformed") else 1

    if args.scenario == "all":
        for name, gen in SCENARIOS.items():
            send_all(sock, target, gen(stamp), args.rate, name)
            stamp += 100_000
        send_all(sock, target, MALFORMED, args.rate, "malformed")
        return 0

    gen = SCENARIOS.get(args.scenario)
    if gen is None:
        print(f"unknown scenario: {args.scenario}", file=sys.stderr)
        print(f"available: {', '.join(sorted(SCENARIOS))}, malformed, all", file=sys.stderr)
        return 2

    # A single pass is a couple of seconds of frames, after which the panel correctly falls to
    # `stale`. That is fine for an assertion and useless for looking at the glass: the manual pass
    # asks whether a colour blend bands, whether a glyph clips, whether a flash rate feels right -
    # none of which can be judged in two seconds followed by a link screen.
    #
    # --loop repeats until interrupted. Stamps keep climbing across repetitions rather than
    # restarting, because a stamp that jumps backwards by less than the staleness threshold is
    # rejected as out-of-order, and one that jumps back further is read as a producer restart.
    # Either would make the panel behave oddly for reasons that have nothing to do with rendering.
    if not args.loop:
        send_all(sock, target, gen(stamp), args.rate, args.scenario)
        return 0

    print(f"  looping {args.scenario} at {args.rate:g} Hz - press Ctrl+C to stop")
    passes = 0
    try:
        while True:
            send_all(sock, target, gen(stamp), args.rate, args.scenario)
            stamp += 100_000
            passes += 1
    except KeyboardInterrupt:
        print(f"\n  stopped after {passes} pass(es)")
    return 0


def load_capture(path) -> list[tuple[int, dict]]:
    """Read a capture into (offsetMs, frame) pairs.

    Extracted from cmd_replay so that tools/check_capture_roundtrip.py can exercise the REAL
    reader rather than a reimplementation of it. The capture writer is C# and this reader is
    Python; they agree because both conform to the same message contract, and the only way to keep
    that true is to test the actual pair rather than two things that resemble them.

    A capture truncated by a crash is still usable up to its last complete line - which is the
    whole reason the format is line-delimited - so a bad line stops the read rather than failing it.
    """
    path = Path(path)
    records: list[tuple[int, dict]] = []
    previous_offset = 0

    with path.open(encoding="utf-8") as fh:
        for line_no, line in enumerate(fh, 1):
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                print(f"  stopping at line {line_no}: {exc}")
                break
            offset = record.get("offsetMs", previous_offset)
            previous_offset = offset
            records.append((offset, record["frame"]))

    return records


def cmd_replay(args) -> int:
    path = Path(args.capture)
    if not path.exists():
        print(f"no such capture: {path}", file=sys.stderr)
        return 2

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = (args.host, args.port)
    sent = 0
    previous_offset = 0

    for offset, frame_obj in load_capture(path):
        if not args.fast:
            time.sleep(max(0.0, (offset - previous_offset) / 1000.0))
        previous_offset = offset
        sock.sendto(json.dumps(frame_obj).encode("utf-8"), target)
        sent += 1

    print(f"  replayed {sent} frame(s) to {target[0]}:{target[1]}")
    return 0


def cmd_listen(args) -> int:
    """Stand in for the PC side: answer registrations the way the publisher does."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", args.port))
    print(f"listening on :{args.port} — registrations will be echoed with a live frame stream")
    devices: dict[tuple[str, int], float] = {}
    stamp = 0
    try:
        sock.settimeout(0.25)
        while True:
            try:
                data, addr = sock.recvfrom(4096)
                try:
                    reg = json.loads(data)
                    print(f"  registration from {addr[0]} "
                          f"device={reg.get('deviceId')} fw={reg.get('firmwareVersion')}")
                except json.JSONDecodeError:
                    print(f"  non-JSON datagram from {addr[0]} ({len(data)} bytes)")
                devices[addr] = time.monotonic()
            except socket.timeout:
                pass

            now = time.monotonic()
            # A device is forgotten after 6 s of silence - three missed keepalives.
            for addr in [a for a, seen in devices.items() if now - seen > 6.0]:
                print(f"  forgetting {addr[0]} (no keepalive for 6 s)")
                devices.pop(addr, None)

            stamp += 16
            payload = json.dumps(frame(stamp)).encode("utf-8")
            for addr in devices:
                sock.sendto(payload, addr)
            time.sleep(0.016)
    except KeyboardInterrupt:
        print("\nstopped")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("synth", help="generate frames a real adapter would never produce")
    s.add_argument("--host", required=True)
    s.add_argument("--port", type=int, default=PORT)
    s.add_argument("--scenario", default="sweep",
                   help=f"one of: {', '.join(sorted(SCENARIOS))}, malformed, all")
    s.add_argument("--rate", type=float, default=60.0, help="datagrams per second")
    s.add_argument("--loop", action="store_true",
                   help="repeat the scenario until interrupted - use this for the manual pass, "
                        "where the panel has to keep showing the thing being judged")
    s.set_defaults(func=cmd_synth)

    r = sub.add_parser("replay", help="send a captured lap")
    r.add_argument("capture")
    r.add_argument("--host", required=True)
    r.add_argument("--port", type=int, default=PORT)
    r.add_argument("--fast", action="store_true", help="ignore timing, send as fast as possible")
    r.set_defaults(func=cmd_replay)

    l = sub.add_parser("listen", help="stand in for the PC side and answer registrations")
    l.add_argument("--port", type=int, default=PORT)
    l.set_defaults(func=cmd_listen)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
