# pcb2gcodeGUI
A simple GUI for pcb2gcode. 
The minimum required Qt's version is 5.0.2.
After building it, download and install pcb2gcode
from http://sourceforge.net/projects/pcb2gcode/ (use the
git version)

## Build
### Build on Debian Jessie/Sid, Ubuntu Trusty/Utopic/Vivid
    
    sudo apt-get install build-essential git qt5-default
    git clone https://github.com/pcb2gcode/pcb2gcodeGUI.git
    cd pcb2gcodeGUI/
    qmake
    make

Now just run the built executable with

    ./pcb2gcodeGUI

### Build on Debian Wheezy
You need wheezy-backports: add this line to /etc/apt/sources.list

	deb http://ftp.debian.org/debian/ wheezy-backports main

and do an apt-update

	sudo apt-get update

then follow the instructions for Debian Jessie

### Build on Windows
The easiest way to build pcb2gcodeGUI on windows is by downloading
the Qt SDK from here http://www.qt.io/download-open-source/ and
building it within Qt Creator. After that, download a pcb2gcode
Windows build and put it in the same folder of the pcb2gcodeGUI
binary
