# pcb2gcodeGUI
A GUI for [pcb2gcode](https://github.com/pcb2gcode/pcb2gcode).

**!!!!If you see a feature missing, either switch to use the command-line pcb2gcode or consider contributing code to this GUI.**

## Build
### Build on Debian Jessie or newer, Ubuntu Trusty or newer
    
    sudo apt-get install build-essential git qt5-default libqt5svg5-dev
    git clone https://github.com/pcb2gcode/pcb2gcodeGUI.git

Then follow the common steps

### Build on Ubuntu Jammy Jellyfish or newer
    sudo apt install build-essential git qmake6 qt6-base-dev qt6-svg-dev
    git clone https://github.com/pcb2gcode/pcb2gcodeGUI.git

Then follow the common steps

### Common steps
    cd pcb2gcodeGUI/
    qmake
    make
    sudo make install

Now follow the installations in pcb2gcode's [README](https://github.com/pcb2gcode/pcb2gcode/blob/master/README.md)
and install pcb2gcode.
Now you can run pcb2gcodeGUI with

    pcb2gcodeGUI

You can also specify a different installation path with

    qmake PREFIX=<prefix path>

pcb2gcodeGUI will be installed in PREFIX/bin

### Build on Debian Wheezy
You need wheezy-backports: add this line to /etc/apt/sources.list

	deb http://ftp.debian.org/debian/ wheezy-backports main

and do an apt-update

	sudo apt-get update

then follow the instructions for Debian Jessie

### Build on Windows
Note that pre-built binaries for Windows are available in the [release page](https://github.com/pcb2gcode/pcb2gcodeGUI/releases).

To build pcb2gcodeGUI on Windows download the [Qt SDK](http://www.qt.io/download-open-source/)
and install it, then open pcb2gcodeGUI.pro inside Qt creator, select "Release" and build it.

You can also build pcb2gcodeGUI statically with [MSYS2](http://sourceforge.net/projects/msys2/):
 * Download MSYS2
 * Install the required build packages (follow the instructions for pcb2gcode)
 * `pacman --needed -S mingw-w64-i686-qt5-static mingw-w64-i686-libwebp mingw-w64-i686-dbus` (or `pacman --needed -S mingw-w64-x86_64-qt5-static mingw-w64-x86_64-libwebp mingw-w64-x86_64-dbus` if you want a 64-bit binary)
 * `git clone https://github.com/pcb2gcode/pcb2gcodeGUI.git`
 * `cd pcb2gcodeGUI/`
 * `/mingw32/qt5-static/bin/qmake.exe` (or `/mingw64/qt5-static/bin/qmake.exe` for the 64-bit binary)
 * `make`
 
Then copy pcb2gcodeGUI.exe and a pcb2gcode binary (download it from the [release page](https://github.com/pcb2gcode/pcb2gcode/releases)
or [build it](https://github.com/pcb2gcode/pcb2gcode/blob/master/README.md)) in the same folder.
