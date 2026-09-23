# cyd-sim-dash

A four-element sim-racing dashboard on an ESP32 **Cheap Yellow Display** (ESP32-2432S028R), fed
over WiFi by a purpose-built SimHub plugin. Mounted above the wheel, it shows four things at once:

- a large **gear** indicator in the centre,
- the whole **background ramping** green → amber → red and then flashing at the shift point,
- a **left edge bar** when a car is alongside on the left,
- a **right edge bar** for the right.

**v1 supports iRacing only.** Assetto Corsa, ACC and Euro Truck Simulator 2 are v2 — iRacing is the
only title where all four elements can work, so it is the only one that tests the concept rather
than a subset of it.

> **Status: in build.** The documentation set is complete and the first slices have landed. Nothing
> is flashable yet. See [`IMPLEMENTATION.md`](IMPLEMENTATION.md) for what is built and what is not.

## How it works

Two first-party components joined by one wire contract:

```
  SimHub  ──►  plugin  ──►  UDP/JSON  ──►  CYD firmware  ──►  panel
   (PC)      (C#, .NET)      (LAN)          (ESP32)
```

The device holds the PC's address and registers itself every 2 seconds; the plugin replies to the
source address and streams frames back. **The plugin holds no configuration at all** — the device's
own address may change freely under DHCP, and there is no broadcast traffic.

Each supported sim is one **title-adapter module** on the PC side. The adapter resolves everything
sim-specific — including the shift-light fallback chain — and emits a normalised frame carrying
absolute RPM thresholds, so **the firmware contains no per-title logic** and adding a sim never
means reflashing the panel.

For iRacing the shift points come from the sim itself: `DriverCarSLFirstRPM` and
`DriverCarSLShiftRPM`, published per car in real RPM, rather than from SimHub's computed
approximation.

## Repository layout

| Path | What |
|---|---|
| `contracts/` | The wire contract — JSON schemas and the fixtures all three implementations test against |
| `firmware/` | ESP32 firmware. `src/display_state.*` is pure and host-compilable |
| `plugin/` | The SimHub plugin (C#, .NET Framework 4.8) |
| `tools/` | `replay.py` drives a device with no SimHub, sim or rig; `check_fixtures.py` is the contract tier |
| `docs/` | The documentation set this is built from — see below |
| `dictum/` | The vendored documentation standard |

## Documentation

This product is specified before it is built, against the [Dictum](https://github.com/pnjacket/dictum)
standard. [`docs/README.md`](docs/README.md) is the index; [`docs/manifest.yaml`](docs/manifest.yaml)
is authoritative. **127 contracts** carry stable IDs, and every one has a named check or a stated
reason it has none.

If the code and the docs disagree, the docs are the contract and the code is wrong.

## Building

Nothing is flashable yet. When it is:

- **Firmware** — Arduino IDE or `arduino-cli`, ESP32 core 3.3.12, with TFT_eSPI 2.5.43,
  ArduinoJson **7.4.3 or later** and WiFiManager 2.0.17. Exact versions matter; see
  [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md).
- **Plugin** — .NET Framework 4.8, referencing three assemblies from your own SimHub installation.

Running the tests today needs only Python and a C++ compiler:

```
python -m pip install jsonschema
python tools/check_fixtures.py     # wire contract, 30 fixtures
cd firmware/test && make test      # display-state engine, unit tier
```

> **TFT_eSPI's pin configuration lives in the library's own header, not in the sketch.** Getting it
> wrong produces a blank screen with no error of any kind. It is the single most likely reason a
> CYD project does not work, and the setup instructions will treat it as such.

## Finding the panel on your network

The panel takes its address from DHCP, so it will not be the same one twice and there is nothing to
write down. It announces itself over mDNS as **`cyd-sim-dash`**, so ask for it by name rather than
hunting for it:

```
ping cyd-sim-dash.local                          # resolves to the current address
python tools/e2e.py --device cyd-sim-dash.local  # the end-to-end harness takes the name too
tools/ota.sh cyd-sim-dash.local <your-pc-ip>     # so does the update script
```

Once it is running, `http://cyd-sim-dash.local/state` returns everything the panel currently
believes, as JSON. That endpoint is in every build including release, and it is the intended way to
read the device: there is no serial cable in normal use.

**A scan for open ports will not find it.** ArduinoOTA listens on **UDP** 3232 and then dials back
to your PC over TCP, so a TCP port scan of 3232 reports *closed* on a device that is working
perfectly. Before the panel has been updated to a build carrying the state endpoint it has no TCP
port open at all, and the only things that will find it are mDNS and ARP.

## Looking at the panel: the manual pass

Some things cannot be asserted from a test. Whether a colour blend bands, whether a glyph clips,
whether the flash rate feels urgent rather than frantic — those need eyes on the glass, and they are
what the manual pass is for.

Drive the panel continuously with `--loop`, which repeats until you interrupt it.

**Only ever run one at a time.** Two senders have independent frame stamps, so the device rejects
about half of everything as out-of-order and the panel jumps between scenarios — which looks like a
firmware fault and is not one. `tools/drive.ps1` stops any existing sender before starting the next,
and `tools/e2e.py` refuses to run while anything is broadcasting:

```
pwsh tools/drive.ps1 gears        # stops whatever is running, then drives this one
pwsh tools/drive.ps1 -StopOnly    # stop everything
```

Run it under **pwsh**, not the older `powershell.exe`, whose default execution policy refuses to
load the file. And note that `pkill -f replay.py` and `kill <pid>` do **not** stop a native Windows
Python process — both report success while it keeps sending.

Or drive a scenario directly:

```
python tools/replay.py synth --host cyd-sim-dash.local --scenario sweep       --loop
python tools/replay.py synth --host cyd-sim-dash.local --scenario gears       --loop
python tools/replay.py synth --host cyd-sim-dash.local --scenario composition --loop
```

Without `--loop` a scenario is a two-second burst, after which the panel correctly falls to
**Telemetry stopped** — that is the staleness rule working, not a dropout, and two seconds is not
long enough to judge anything.

What to look for, one scenario at a time:

| Scenario | Watch for |
|---|---|
| `sweep` | the background blending green to amber to red with no visible steps, starting exactly at the ramp threshold; then flashing above it |
| `gears` | every gear legible and inside the centre region — including `N`, `R` and a two-character gear, which uses a smaller face so it cannot clip |
| `composition` | a lit bar staying **solid white through both phases of the flash**, never suppressed by the background |

The flash rate is 3 Hz and is not resolvable by eye at that precision. Measuring it properly needs a
slow-motion video at a known frame rate — a phone at 240 fps is adequate — counting frames between
transitions.

## Changing the sim PC address later

Open **`http://cyd-sim-dash.local/`** from any machine on the same network. The page asks for the
device credential; the username is **`admin`** and the password is the credential you set when
provisioning. One shared credential gates this page and firmware updates alike.

Leave the credential field **blank to keep the current one** — the form says so, because an
operator changing only the PC address should not have to retype a secret to avoid wiping it.

The page also shows the soft-fault counters since boot, which are the first thing worth looking at
when frames are arriving but nothing appears, and carries an erase action that overwrites the
stored values before deleting them.

## Licence

MIT — see [`LICENSE`](LICENSE). Distributed as **source only**; no prebuilt firmware binary is
published, which is what keeps the LGPL-2.1 ESP32 core's obligations satisfied by construction.
Third-party licences are listed in [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md).
