#!/bin/sh
# Regenerates resources/pcb2gcode-options.json, the option list the GUI uses when the
# installed pcb2gcode can't describe its own options.
#
#   tools/update-option-snapshot.sh PCB2GCODE [PCB2GCODE_SOURCE_DIR]
#
# With a pcb2gcode that has --dump-options, its output is the snapshot. Older versions
# need their source tree, whose src/options.cpp is parsed instead.
# Afterwards, place new options in resources/gui-layout.json (tst_core checks this).
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/resources/pcb2gcode-options.json"
pcb2gcode=${1:?usage: $0 PCB2GCODE [PCB2GCODE_SOURCE_DIR]}

if "$pcb2gcode" --dump-options --noconfigfile > "$out.tmp" 2>/dev/null; then
  mv "$out.tmp" "$out"
  echo "Wrote $out from $pcb2gcode --dump-options"
  exit 0
fi
rm -f "$out.tmp"

src=${2:-}
if [ -z "$src" ]; then
  echo "$pcb2gcode has no --dump-options; pass the pcb2gcode source directory as well." >&2
  exit 1
fi
python3 "$root/tools/snapshot_from_options_cpp.py" "$src" "$out"
