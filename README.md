# pcb2gcodeGUI

A graphical front end for [pcb2gcode](https://github.com/pcb2gcode/pcb2gcode): pick your Gerber and
Excellon files, set the milling, drilling and cutting options, preview the toolpaths, and write the
G-code.

## How the options stay in sync with pcb2gcode

The option form isn't drawn by hand. It is generated from a description of pcb2gcode's options:
type, default, units, range, choices, group and help text.

1. If the installed pcb2gcode can describe its own options (`pcb2gcode --dump-options`, in
   pcb2gcode master after 3.2.0), the GUI uses that, so it always matches the pcb2gcode you run.
   New options, units and checks in later versions need no change here.
2. Otherwise it uses its built-in copy of that description,
   [`resources/pcb2gcode-options.json`](resources/pcb2gcode-options.json), which describes
   pcb2gcode 3.2.0, the last release without `--dump-options`. It checks it against
   `pcb2gcode --help`: options the installed pcb2gcode doesn't have are hidden, and options the GUI
   doesn't know are added as plain fields on an "Other" tab.
3. Without pcb2gcode, the GUI still opens, so configurations can be edited; it asks where pcb2gcode is.

[`resources/gui-layout.json`](resources/gui-layout.json) only arranges the options into tabs and
sections and gives them short labels, starting values and "enabled when" rules. An option that
isn't listed there still appears, in the tab of its pcb2gcode group.

The status bar says which description is in use.

## Features

- Every option of pcb2gcode 3.2.0, including multi-step milldrilling, several mill diameters,
  autolevelling, tiling and bridges.
- Values are passed with explicit units (`--zwork=-0.05mm`, `--mill-feed=600mm/min`,
  `--spinup-time=1.5s`); switching between millimetres and inches converts the fields.
- Only values that differ from pcb2gcode's defaults are passed; their labels are bold. Right-click a
  label to reset it. Optional values without a default (plunge rate, depth per pass, bridge height…)
  have a check box: unticked means pcb2gcode decides.
- Live previews: input layers, toolpaths per tool, before and after optimisation, clearance
  problems, drilled and milldrilled holes. Mouse wheel zooms, dragging pans.
- Configuration files use pcb2gcode's own `millproject` format, so a saved file also works with
  `pcb2gcode --config=FILE`. Files written by older versions of the GUI load, and deprecated options
  are converted (`drill-front`, `milldrill`, `offset` as the tool radius…).
- *Run → Show command line* shows the exact pcb2gcode command, ready to copy.

## Building

pcb2gcodeGUI needs CMake 3.16 or newer, a C++17 compiler and Qt 6.2 or newer (Core, Gui, Widgets,
Svg, SvgWidgets). Then:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    sudo cmake --install build      # or run build/pcb2gcodeGUI directly

Install pcb2gcode as explained in its [README](https://github.com/pcb2gcode/pcb2gcode#readme).
pcb2gcodeGUI looks for it next to its own executable, on `PATH`, and in the usual Homebrew
locations; *Settings → pcb2gcode executable…* points it elsewhere.

### Debian, Ubuntu (22.04 or newer)

    sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev libgl1-mesa-dev

### Fedora

    sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtsvg-devel

### macOS

    brew install cmake qt
    cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"

`cmake --build build --target package` makes a disk image with Qt included.

### Windows

With [MSYS2](https://www.msys2.org/) (UCRT64 shell):

    pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
        mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-svg

Or install Qt with the [Qt online installer](https://www.qt.io/download-qt-installer-oss) and open
`CMakeLists.txt` in Qt Creator. Put `pcb2gcode.exe` from the
[pcb2gcode releases](https://github.com/pcb2gcode/pcb2gcode/releases) next to `pcb2gcodeGUI.exe`.

## Command line

    pcb2gcodeGUI [millproject]                       open a configuration file
    pcb2gcodeGUI --pcb2gcode /path/to/pcb2gcode      use a specific pcb2gcode
    pcb2gcodeGUI --print-command-line millproject    print the pcb2gcode arguments and exit
    pcb2gcodeGUI --check-previews DIR                list and check the preview images in DIR

## Tests

    cmake -S . -B build -DPCB2GCODE_EXECUTABLE=/path/to/pcb2gcode
    cmake --build build
    ctest --test-dir build --output-on-failure

With a pcb2gcode, the tests also run complete jobs on the boards in `tests/data`, checking the
command line, the G-code files and the previews. They check that the GUI can read that pcb2gcode's
`--dump-options`, or, for a pcb2gcode without it, that the built-in option list matches its
`--help`.

## The built-in option list

`resources/pcb2gcode-options.json` is only used with pcb2gcode that can't describe its own options,
so it stays a description of pcb2gcode 3.2.0 and doesn't follow newer versions. It was made from
pcb2gcode's source with:

    tools/update-option-snapshot.sh /path/to/pcb2gcode-3.2.0-source

An option added to pcb2gcode shows up in the tab of its group; give it a place and a label in
`resources/gui-layout.json` if it deserves one.
