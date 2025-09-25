#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
PROJ_DIR="${2:-$HOME/esp/udp_echo_ap}"

if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
  . "$HOME/esp/esp-idf/export.sh"
else
  echo "ERROR: ESP-IDF not found at ~/esp/esp-idf/export.sh" >&2
  exit 1
fi

cd "$PROJ_DIR"

sed -i -E 's|^(#define[[:space:]]+USE_11B_ONLY)[[:space:]]+[01]|\1   1|' main/main.c

idf.py set-target esp32c6
idf.py build
idf.py -p "$PORT" flash
echo "Flashed in 802.11b (DSSS) mode to $PORT"
