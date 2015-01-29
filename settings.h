/*
 * Copyright (c) 2015 Nicola Corna (nicola@corna.info)
 *
 * This file is part of pcb2gcodeGUI.
 *
 * pcb2gcodeGUI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pcb2gcodeGUI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcodeGUI.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SETTINGS
#define SETTINGS

#include <QObject>

#ifdef Q_OS_WIN
#define PCB2GCODE_COMMAND_NAME "pcb2gcode.exe"
#define PCB2GCODE_EXECUTABLE PCB2GCODE_COMMAND_NAME
#else
#define PCB2GCODE_COMMAND_NAME "pcb2gcode"
#define PCB2GCODE_EXECUTABLE PCB2GCODE_COMMAND_NAME
#endif

#define PCB2GCODE_GUI_VERSION 0.1

static const char *about_pcb2gcode_str = "pcb2gcode v%1<br>"
                                         "<a href='http://sourceforge.net/projects/pcb2gcode/'>http://sourceforge.net/projects/pcb2gcode/</a><br>";
static const char *about_pcb2gcodegui_str = "pcb2gcodeGUI v%1<br>"
                                            "Created by Nicola Corna (nicola@corna.info)<br>"
                                            "<a href='https://github.com/Corna/pcb2gcodeGUI'>https://github.com/Corna/pcb2gcodeGUI</a>";
static const char *manual_link = "https://github.com/patkan/pcb2gcode/wiki/Manual";

static const char *default_config_filename = "/default_millproject";

static const char *gerber_file_filter = "Gerber files (RS-274X) (*.gbr *.gtl *.gtp *.gto *.gbs *.gts);;All types (*.*)";
static const char *excellon_file_filter = "Excellon files (*.drl);;All types (*.*)";
static const char *gcode_file_filter = "G-Code files (*.nc *.cnc *.ncc *.ecs *.ncg *.fan *.hnc);;All types (*.*)";
static const char *text_file_filter = "Text files (*.*)";

#endif // SETTINGS

