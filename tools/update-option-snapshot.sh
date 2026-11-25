#!/bin/sh
# Regenerates resources/pcb2gcode-options.json, the option list the GUI uses with a pcb2gcode
# that can't describe its own options. It describes pcb2gcode 3.2.0, the last release without
# --dump-options: pcb2gcode from then on prints its own list, which the GUI reads instead, so
# newer versions need no update here.
#
#   tools/update-option-snapshot.sh PCB2GCODE_SOURCE_DIR
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
src=${1:?usage: $0 PCB2GCODE_SOURCE_DIR}

if grep -q '"dump-options"' "$src/src/options.cpp"; then
  echo "$src has --dump-options, so the GUI reads that pcb2gcode's options from it." >&2
  echo "The built-in list should keep describing the last release without it." >&2
  exit 1
fi
python3 "$root/tools/snapshot_from_options_cpp.py" "$src" "$root/resources/pcb2gcode-options.json"
