/*
 * Copyright (c) 2026 pcb2gcodeGUI contributors
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

#ifndef PCB2GCODE_H
#define PCB2GCODE_H

#include "optionschema.h"

#include <QString>
#include <QStringList>

namespace pcb2gcode {

// The option that makes pcb2gcode print its option metadata as JSON.
extern const char *const kDumpOptionsFlag;

QString executableName();

// Finds pcb2gcode: the configured path, next to the GUI, on PATH, then in common
// install locations (Finder-started macOS apps don't get the shell's PATH).
QString locate(const QString &configuredPath = QString());

struct Probe {
    QString program;      // absolute path, empty if pcb2gcode wasn't found
    QString version;      // e.g. "3.2.0"
    QString versionText;  // everything --version printed
    OptionSchema schema;
    QString schemaNote;   // where the option list came from, for the status bar
    bool olderThanSchema = false;
};

// Asks `program` (which may be empty) for its version and options.
Probe probe(const QString &program, bool forceBundled = false);

// Runs a short command and returns its standard output; false on failure or timeout.
bool run(const QString &program, const QStringList &arguments, int timeoutMs, QByteArray *output,
         int *exitCode = nullptr, QString *error = nullptr);

// Compares dotted versions: negative, zero or positive like strcmp.
int compareVersions(const QString &a, const QString &b);

// The command as it would be typed in this platform's shell.
QString shellCommand(const QString &program, const QStringList &arguments);

}  // namespace pcb2gcode

#endif  // PCB2GCODE_H
