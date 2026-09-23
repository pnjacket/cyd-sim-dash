#!/usr/bin/env bash
# Build and push firmware over the air.
#
# Two details make the difference between this working and hanging:
#
#   -I <host ip>   espota asks the device to connect BACK to the host over TCP. Left to default it
#                  advertises 0.0.0.0, and on a machine with several interfaces the device dials a
#                  dead address and the upload times out with "No response from device".
#   -include       TFT_eSPI is not header-only. Its own .cpp compiles as a separate translation
#                  unit, so the panel configuration has to be forced into every TU or the library
#                  silently builds with its default pins - which looks like a white screen.
#
# The device credential gates OTA (SEC-CREDENTIAL-POLICY: one shared credential for the
# configuration page and the update path). Pass it as the third argument or in CYD_CREDENTIAL.
# Without it espota fails authentication on any device that has one set - which, per the policy,
# is every device that was provisioned correctly.
#
# Usage: tools/ota.sh [device-ip] [host-ip] [credential]
#    or: CYD_CREDENTIAL=... tools/ota.sh cyd-sim-dash.local

set -euo pipefail
cd "$(dirname "$0")/.."

# mDNS by default: the panel announces itself as cyd-sim-dash, and its DHCP address is not
# something anyone should have to keep track of.
DEVICE_IP="${1:-cyd-sim-dash.local}"
HOST_IP="${2:-$(powershell.exe -NoProfile -Command \
  "(Get-NetIPAddress -AddressFamily IPv4 | Where-Object { \$_.IPAddress -like '192.168.*' } | Select-Object -First 1).IPAddress" \
  2>/dev/null | tr -d '\r')}"

CREDENTIAL="${3:-${CYD_CREDENTIAL:-}}"

FQBN="esp32:esp32:esp32:PartitionScheme=min_spiffs"
CFG="$(pwd -W 2>/dev/null || pwd)/firmware/src/tft_config.h"
ESPOTA="$HOME/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.12/tools/espota.exe"

echo "building..."
rm -rf build
arduino-cli compile --fqbn "$FQBN" --output-dir build \
  --build-property "compiler.cpp.extra_flags=-include \"$CFG\"" firmware | tail -2

echo "pushing to $DEVICE_IP from $HOST_IP ..."
if [ -n "$CREDENTIAL" ]; then
  "$ESPOTA" -I "$HOST_IP" -P 45678 -i "$DEVICE_IP" -p 3232 -a "$CREDENTIAL" -f build/firmware.ino.bin -r
else
  echo "  (no credential supplied - this only works on a device that has none set)"
  "$ESPOTA" -I "$HOST_IP" -P 45678 -i "$DEVICE_IP" -p 3232 -f build/firmware.ino.bin -r
fi
