# Third-party notices

`cyd-sim-dash` is distributed under the MIT licence (see `LICENSE`). It is distributed **as source
only**; no prebuilt firmware binary is published. The dependencies below are installed by the
builder, not redistributed by this project.

This file mirrors the inbound licence register in
[`docs/governance-and-compliance.md`](docs/governance-and-compliance.md). If the two disagree, the
doc-set register is authoritative.

## Device libraries

| Dependency | Version | Licence |
|---|---|---|
| [arduino-esp32](https://github.com/espressif/arduino-esp32) (ESP32 Arduino core) | 3.3.12 | **LGPL-2.1**. See the note below |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | 2.5.43 | **Three licences** — MIT for code derived from Adafruit_ILI9341, BSD for code derived from Adafruit_GFX, and FreeBSD for Bodmer's own work and examples. © 2026 Bodmer; Adafruit portions © Limor Fried / Adafruit Industries |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.4.3 or later | MIT, © 2014–2026 Benoit Blanchon |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | 2.0.17 | MIT, © 2015 tzapu |

**ArduinoJson 7.4.3 is a floor, not merely a pin.** Versions through 7.4.2 carry a buffer overrun
in string-to-float conversion reachable by a JSON string of many digits. This firmware parses
datagrams from an untrusted LAN, so no build may go below that version. See `SEC-PARSER-FLOOR`.

**Note on the ESP32 Arduino core.** The repository declares LGPL-2.1, and the distribution bundles
third-party components under their own terms — ESP-IDF components are Apache-2.0. Because this
project distributes **source only**, the builder performs the link on their own machine and no
combined work is conveyed by this project.

> [GAP] The bundled components' own notices are not yet enumerated here. That enumeration is owed
> once the core is installed, and is required for `G1` to be fully satisfiable.

## PC-side

| Dependency | Version | Licence |
|---|---|---|
| SimHub | 9.11.13 (tested) | Proprietary. **Not redistributed.** The plugin references `GameReaderCommon.dll`, `SimHub.Logging.dll` and `SimHub.Plugins.dll` from the builder's own SimHub installation, with `Private="False"` so they are never copied into build output |
| iRacing | n/a | Proprietary. A runtime data source reached only through SimHub; no code from it enters this product |
| Python | any 3.x | PSF. Development tooling; not distributed with the product |
