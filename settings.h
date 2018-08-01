/*
 * Copyright (c) 2015-2016 Nicola Corna (nicola@corna.info)
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
#include <QVector>

#ifdef Q_OS_WIN
#define PCB2GCODE_COMMAND_NAME "pcb2gcode.exe"
#define PCB2GCODE_EXECUTABLE PCB2GCODE_COMMAND_NAME
#else
#define PCB2GCODE_COMMAND_NAME "pcb2gcode"
#define PCB2GCODE_EXECUTABLE PCB2GCODE_COMMAND_NAME
#endif

#define PCB2GCODE_GUI_VERSION "1.3.2-1"

static const char *about_pcb2gcode_str = "pcb2gcode v%1<br>"
                                         "<a href='https://github.com/pcb2gcode/pcb2gcode'>https://github.com/pcb2gcode/pcb2gcode</a><br>";
static const char *about_pcb2gcodegui_str = "pcb2gcodeGUI v%1<br>"
                                            "Created by Nicola Corna (nicola@corna.info)<br>"
                                            "<a href='https://github.com/pcb2gcode/pcb2gcodeGUI'>https://github.com/pcb2gcode/pcb2gcodeGUI</a>";
static const char *manual_link = "https://github.com/pcb2gcode/pcb2gcode/wiki/Manual";

static const char *default_config_filename = "/default_millproject";

static const QString gerber_front_file_filter("Gerber front files (*.gbr *.gtl *.gml);;");
static const QString gerber_back_file_filter("Gerber back files (*.gbr *.gbl *.gml);;");
static const QString gerber_outline_file_filter("Gerber outline files (*.gbr *.gm1);;");

static const QString gerber_file_filter = "Gerber files (*.gbl *.gbs *.gbo *.gbp *.gko *.gm1 *.gm2 *.g1 *.g2 *.gp1 *.gp2 *.gpb *.gpt *.gtp *.gtl *.gts *.gto *.gbr *.grb *.gml);;All types (*.*)";
static const QString excellon_file_filter = "Excellon files (*.drl *.drd *.dri *.txt);;All types (*.*)";
static const QString gcode_file_filter = "G-Code files (*.nc *.cnc *.ncc *.ecs *.ngc *.fan *.hnc);;All types (*.*)";
static const QString text_file_filter = "Text files (*.*)";

static const QVector<int> targetVersion(QVector<int>() << 1 << 3 << 2);

#endif // SETTINGS
