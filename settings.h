#ifndef SETTINGS
#define SETTINGS

#include <QObject>

#ifdef Q_OS_WIN
#define PCB2GCODE_COMMAND_NAME "pcb2gcode.exe"
#define PCB2GCODE_EXECUTABLE "pcb2gcode/" PCB2GCODE_COMMAND_NAME
#else
#define PCB2GCODE_COMMAND_NAME "pcb2gcode"
#define PCB2GCODE_EXECUTABLE PCB2GCODE_COMMAND_NAME
#endif

#define PCB2GCODE_GUI_VERSION 0.1

static const char *about_pcb2gcode_str = "pcb2gcode v%1<br>"
                                         "Based on <a href='http://sourceforge.net/projects/pcb2gcode/'>http://sourceforge.net/projects/pcb2gcode/</a><br>"
                                         "Currently developed on <a href='https://github.com/paktan/pcb2gcode'>https://github.com/paktan/pcb2gcode</a>";
static const char *about_pcb2gcodegui_str = "pcb2gcodeGUI v%1<br>"
                                            "Created by Nicola Corna (nicola@corna.info)<br>"
                                            "<a href='https://github.com/Corna/pcb2gcodeGUI'>https://github.com/Corna/pcb2gcodeGUI</a>";
static const char *manual_link = "https://github.com/patkan/pcb2gcode/wiki/Manual";

static const char *default_config_filename = "/default_millproject";

#endif // SETTINGS

