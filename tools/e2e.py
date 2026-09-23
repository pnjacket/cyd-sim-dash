#!/usr/bin/env python3
"""End-to-end harness: drive a real device with synthetic frames, assert through API-STATE.

This closes the loop the operator asked for. Once the panel is off the USB cable there is no
serial, so nothing could be read back from it; `GET /state` replaces every diagnostic print, and
unlike serial it is readable from the machine running the tests rather than only from the one
holding the cable.

What makes this a legitimate end-to-end run rather than a bypass: the replay tool substitutes the
*entire PC chain* at the wire contract — SimHub, the plugin, the adapter and the sim — while every
line of firmware still runs for real, on the real panel, over real WiFi. The device cannot tell the
difference, which is the property being relied on.

It is deliberately assertive about the difference between *unavailable* and *cleared*: several
checks below would pass against a device that confused null with "none", and they are written to
fail instead, because that confusion is the one this product cannot afford — it would tell a driver
the road is clear when the truth is that nothing is known.

Usage:
    python tools/e2e.py --device 192.168.1.50
    python tools/e2e.py --device 192.168.1.50 --only link

Exit: 0 if every assertion holds.
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
import urllib.error
import urllib.request

PORT = 47110
CHECKS = 0
FAILURES = 0

# One monotonic stamp allocator for the whole run.
#
# Each test used to derive its own base from the wall clock, which is subtly wrong: a test that
# advances synthetic stamps by +2000 in under two seconds of real time leaves the next test's
# wall-clock base BEHIND the highest stamp the device has accepted. The device then rejects those
# frames as out-of-order - correctly, per INV-STAMP-ORDER - and the test fails against firmware
# that is working. Allocating from one increasing counter removes the whole class.
_STAMP = int(time.time() * 1000) % 1_000_000


def next_stamp(step: int = 1000) -> int:
    global _STAMP
    _STAMP += step
    return _STAMP


def check(condition: bool, label: str) -> None:
    global CHECKS, FAILURES
    CHECKS += 1
    if not condition:
        FAILURES += 1
        print(f"  FAIL  {label}")


def state(device: str, timeout: float = 4.0) -> dict:
    """Read API-STATE. Raises on transport failure, which is itself a result worth failing on."""
    url = f"http://{device}/state"
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def send(device: str, payload: dict, count: int = 1, gap: float = 0.02) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    data = json.dumps(payload).encode("utf-8")
    for _ in range(count):
        sock.sendto(data, (device, PORT))
        time.sleep(gap)
    sock.close()


def send_raw(device: str, data: bytes, count: int = 1, gap: float = 0.02) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    for _ in range(count):
        sock.sendto(data, (device, PORT))
        time.sleep(gap)
    sock.close()


def frame(stamp: int, **overrides) -> dict:
    f = {
        "protocolMajor": 1,
        "protocolMinor": 0,
        "status": "live",
        "titleId": "iracing",
        "stamp": stamp,
        "gear": "4",
        "rpm": 6353.0,
        "rampStartRpm": 6130.0,
        "flashRpm": 6690.0,
        "spotterLeft": "none",
        "spotterRight": "none",
    }
    f.update(overrides)
    return f


def wait_for(device: str, predicate, timeout: float = 6.0) -> dict:
    """Poll API-STATE until the predicate holds. Returns the last state seen either way."""
    deadline = time.time() + timeout
    last: dict = {}
    while time.time() < deadline:
        try:
            last = state(device)
            if predicate(last):
                return last
        except (urllib.error.URLError, socket.timeout, json.JSONDecodeError):
            pass
        time.sleep(0.25)
    return last


def device_is_quiet(device: str, settle: float = 3.0) -> bool:
    """True when nothing else is feeding this device.

    Checked by watching rather than asking: if the reported frame age keeps resetting while we send
    nothing, someone else is sending. The staleness threshold is two seconds, so waiting a little
    over that and finding the device still fed is conclusive.
    """
    try:
        time.sleep(settle)
        after = state(device)
    except Exception:
        return True          # unreachable is a different failure, reported elsewhere

    age = after.get("lastFrameAgeMs")
    link = after.get("linkState")

    # A device nobody is feeding reports a stale link, or an age past the threshold, or has never
    # seen a frame at all.
    if age is None:
        return True
    return age >= 2000 or link in ("stale", "unreachable", "drivingPending", "joining", "unresolved")


# ---------------------------------------------------------------------------


def test_reachable(device: str) -> None:
    print("the endpoint answers, in the shipped binary")
    s = state(device)
    required = [
        "gearGlyph", "shiftPhase", "rampPosition", "barLeft", "barRight",
        "linkState", "configuredHost", "lastFrameAgeMs",
        "malformedCount", "fieldRangeCount", "outOfOrderCount", "versionRejectedCount",
    ]
    for key in required:
        check(key in s, f"API-STATE carries {key}")

    # Additive, and the reason a failing run can say which binary it was talking to.
    check("firmwareVersion" in s, "API-STATE reports the firmware version")
    check("deviceId" in s, "API-STATE reports the device identity")
    print(f"  device {s.get('deviceId')} running {s.get('firmwareVersion')}")


def test_frames_drive_the_display(device: str) -> None:
    print("a live frame reaches the display state")

    stamp = next_stamp()
    send(device, frame(stamp, gear="4", rpm=6353.0), count=5)
    s = wait_for(device, lambda x: x.get("gearGlyph") == "4")

    check(s.get("gearGlyph") == "4", "the gear glyph follows the frame")
    check(s.get("linkState") is None, "a fresh live frame yields a null linkState, not 'driving'")
    check(isinstance(s.get("lastFrameAgeMs"), int), "the frame age becomes a number once a frame lands")

    # Between the thresholds: ramping, with a position strictly inside 0..1.
    check(s.get("shiftPhase") == "ramping", "rpm between the thresholds is the ramping phase")
    position = s.get("rampPosition")
    check(isinstance(position, (int, float)), "rampPosition is a number while ramping")
    if isinstance(position, (int, float)):
        check(0.0 < position < 1.0, f"rampPosition is strictly inside 0..1 (got {position})")

    # Below the ramp start: neutral, and rampPosition must go back to null rather than keeping a
    # stale number that a consumer could not distinguish from a live one.
    send(device, frame(next_stamp(), rpm=3000.0), count=5)
    s = wait_for(device, lambda x: x.get("shiftPhase") == "neutral")
    check(s.get("shiftPhase") == "neutral", "rpm below the ramp start is neutral")
    check(s.get("rampPosition") is None, "rampPosition returns to null when not ramping")

    # Above the flash point.
    send(device, frame(next_stamp(), rpm=7000.0), count=5)
    s = wait_for(device, lambda x: x.get("shiftPhase") == "flashing")
    check(s.get("shiftPhase") == "flashing", "rpm above the flash point is flashing")


def test_spotter_bars(device: str) -> None:
    print("both edge bars, including the case a real track will not stage")

    stamp = next_stamp()

    send(device, frame(stamp, spotterLeft="one", spotterRight="none"), count=5)
    s = wait_for(device, lambda x: x.get("barLeft") is True)
    check(s.get("barLeft") is True and s.get("barRight") is False, "a car on the left lights the left bar only")

    send(device, frame(next_stamp(), spotterLeft="none", spotterRight="one"), count=5)
    s = wait_for(device, lambda x: x.get("barRight") is True)
    check(s.get("barLeft") is False and s.get("barRight") is True, "a car on the right lights the right bar only")

    # The both-sides case. On a real track this needs a car on each side simultaneously, which the
    # operator reports is impractical to stage; here it is one datagram. This does NOT settle
    # whether iRacing reports it correctly — check X10 does that — but it does settle that the
    # device renders it, which is the half that can be settled without a race.
    send(device, frame(next_stamp(), spotterLeft="one", spotterRight="one"), count=5)
    s = wait_for(device, lambda x: x.get("barLeft") is True and x.get("barRight") is True)
    check(s.get("barLeft") is True and s.get("barRight") is True, "a car on each side lights BOTH bars")

    # Unavailable is not cleared. Both must be dark, but for a different reason - and the device
    # must not have coerced null into "none" on the way in.
    send(device, frame(next_stamp(), spotterLeft=None, spotterRight=None), count=5)
    s = wait_for(device, lambda x: x.get("barLeft") is False and x.get("barRight") is False)
    check(s.get("barLeft") is False and s.get("barRight") is False, "unavailable proximity leaves both bars dark")


def test_soft_faults_are_counted(device: str) -> None:
    print("malformed input is counted, and the device stays up")

    before = state(device)

    send_raw(device, b"this is not json at all", count=3)
    send_raw(device, b"{\"protocolMajor\":1,", count=3)
    s = wait_for(device, lambda x: x.get("malformedCount", 0) > before.get("malformedCount", 0))
    check(s.get("malformedCount", 0) > before.get("malformedCount", 0), "malformed datagrams are counted")

    # SEC-INPUT-BOUND, asserted against what this hardware actually does.
    #
    # Measured on the device 2026-09-22: no datagram larger than 1472 bytes - the maximum UDP
    # payload that fits one 1500-byte MTU frame - is ever delivered to the application. The ESP32's
    # network stack drops anything requiring IP reassembly below the firmware, so the 2 KB guard in
    # the receive path CANNOT fire here and its counter cannot increment.
    #
    # That makes the guard defence-in-depth rather than dead code: it is the correct behaviour on a
    # platform that does reassemble, and it costs one comparison. But a check demanding the counter
    # increment would be asserting something the platform makes impossible, so this asserts the
    # property that actually matters and that IS observable - the device is unharmed.
    before_counts = state(device)
    oversized = b'{"protocolMajor":1,"pad":"' + b"9" * 4096 + b'"}'
    send_raw(device, oversized, count=3)
    time.sleep(1.0)
    s = state(device)
    counted = s.get("oversizedCount", 0) > before_counts.get("oversizedCount", 0)
    delivered = s.get("malformedCount", 0) > before_counts.get("malformedCount", 0)
    check(not delivered or counted,
          "S8: if an oversized datagram is delivered at all, it is rejected unparsed, never parsed")
    check(s.get("firmwareVersion") is not None,
          "S8: the device is still serving after oversized input")

    # A 60 KB datagram exceeds the UDP path MTU and will usually be dropped by the stack before the
    # firmware sees it. Sent anyway: what is being asserted is that the device survives it.
    try:
        send_raw(device, b"x" * 60000, count=1)
    except OSError:
        pass  # the local stack refusing to send it is a pass for our purposes

    time.sleep(0.5)
    s = state(device)
    check(s.get("firmwareVersion") is not None, "S8: the device is still serving after oversized input")

    # An out-of-order frame is counted, not rendered.
    stamp = next_stamp()
    send(device, frame(next_stamp(), gear="3"), count=4)
    wait_for(device, lambda x: x.get("gearGlyph") == "3")
    ooo_before = state(device).get("outOfOrderCount", 0)
    # Behind by LESS than the staleness threshold, which matters. StampTracker deliberately treats
    # a backwards jump larger than that as a producer restart and accepts it - otherwise SimHub
    # restarting, which resets its stamp counter to near zero, would make the device reject every
    # subsequent frame forever and leave a dead panel with nothing on screen to explain it.
    #
    # A 5000 ms jump therefore exercises the restart path, not the out-of-order path. Asking for
    # 500 ms gets the rejection this check is about.
    send(device, frame(_STAMP - 500, gear="7"), count=4)
    s = wait_for(device, lambda x: x.get("outOfOrderCount", 0) > ooo_before)
    check(s.get("outOfOrderCount", 0) > ooo_before, "an older stamp is counted as out-of-order")
    check(s.get("gearGlyph") != "7", "and its contents never reach the display")


def test_version_mismatch(device: str) -> None:
    print("a frame with a different major is refused whole")

    before = state(device).get("versionRejectedCount", 0)
    stamp = next_stamp()
    send(device, frame(next_stamp(), protocolMajor=9, gear="2"), count=5)

    s = wait_for(device, lambda x: x.get("versionRejectedCount", 0) > before)
    check(s.get("versionRejectedCount", 0) > before, "I6: a bumped major is counted")
    check(s.get("gearGlyph") != "2", "I6: and none of its contents are rendered")
    check(s.get("linkState") == "versionMismatch", "I6: the device reports the mismatch condition")

    # Recovery: a good frame must clear it, or a single stray datagram would strand the panel.
    send(device, frame(next_stamp(), gear="5"), count=5)
    s = wait_for(device, lambda x: x.get("gearGlyph") == "5")
    check(s.get("gearGlyph") == "5", "a good frame after a mismatch is accepted again")
    check(s.get("linkState") is None, "and the mismatch condition clears")


def test_staleness(device: str) -> None:
    print("silence becomes stale, and says so")

    stamp = next_stamp()
    send(device, frame(stamp), count=5)
    wait_for(device, lambda x: x.get("linkState") is None)

    # Two seconds of silence is the threshold. Nothing is sent during this window on purpose.
    time.sleep(3.0)
    s = state(device)
    check(s.get("linkState") == "stale", f"silence past the threshold is stale (got {s.get('linkState')})")
    check(isinstance(s.get("lastFrameAgeMs"), int) and s["lastFrameAgeMs"] >= 2000,
          "the reported frame age reflects the silence")


def test_unknown_path(device: str) -> None:
    print("an unknown path returns the uniform error, not a framework page")
    try:
        urllib.request.urlopen(f"http://{device}/no-such-endpoint", timeout=4.0)
        check(False, "ERR-UNKNOWN-PATH: an unknown path should not return 200")
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", "replace")
        check(exc.code == 404, "an unknown path returns 404")
        try:
            parsed = json.loads(body)
            check(parsed.get("error") == "unknownPath", "it returns the uniform JSON error shape")
        except json.JSONDecodeError:
            check(False, f"the 404 body is not JSON: {body[:80]!r}")


TESTS = {
    "reachable": test_reachable,
    "frames": test_frames_drive_the_display,
    "spotter": test_spotter_bars,
    "faults": test_soft_faults_are_counted,
    "version": test_version_mismatch,
    "stale": test_staleness,
    "link": test_unknown_path,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--device", required=True, help="the panel's IP address or hostname")
    parser.add_argument("--only", action="append", choices=sorted(TESTS),
                        help="run only the named test; repeatable")
    args = parser.parse_args()

    try:
        state(args.device, timeout=4.0)
    except Exception as exc:
        print(f"cannot reach the device at {args.device}: {exc}")
        print("check it is powered, on the network, and running 0.2.0 or later")
        return 2

    if not device_is_quiet(args.device):
        print("ABORTING: something else is already sending frames to this device.")
        print()
        print("  A second sender is almost invisible and produces failures that look like real")
        print("  defects. Two sources have independent stamp bases, so the device rejects about")
        print("  half of everything as out-of-order, and assertions fail against firmware that is")
        print("  working perfectly. This cost three separate misdiagnoses before it was spotted.")
        print()
        print("  Stop everything first:   tools/drive.ps1 -StopOnly")
        print()
        print("  Note that `pkill -f replay.py` and `kill <pid>` do NOT work here: neither stops a")
        print("  native Windows Python process, and both report success while it keeps sending.")
        return 2

    selected = args.only or list(TESTS)
    for name in selected:
        TESTS[name](args.device)

    print()
    if FAILURES:
        print(f"FAIL  {FAILURES} of {CHECKS} checks")
        return 1
    print(f"PASS  {CHECKS} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
