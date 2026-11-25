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

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <QList>
#include <QString>
#include <QStringList>

class OptionSchema;

// A pcb2gcode configuration file ("millproject"): key=value lines, '#' starts a comment.
// Lines starting with "#@#" hold options that are switched off in the GUI: pcb2gcode
// ignores them, the GUI remembers their values.
struct ConfigEntry {
    QString key;
    QString value;
    bool disabled = false;
    QString section;  // written as a comment above the first entry of each section
};

struct ConfigFile {
    QList<ConfigEntry> entries;

    static ConfigFile parse(const QString &text);
    static ConfigFile load(const QString &path, QString *error = nullptr);
    QString serialize(const QString &header = QString()) const;
    bool save(const QString &path, const QString &header, QString *error = nullptr) const;

    QStringList values(const QString &key, bool disabled = false) const;
    bool contains(const QString &key) const;
};

// Rewrites options of older pcb2gcode versions (drill-front, milldrill, offset as the
// tool radius, ...) into their current form, like pcb2gcode's own fix_variables_map().
// Returns one note per change.
QStringList migrateLegacyEntries(QList<ConfigEntry> &entries, const OptionSchema &schema);

#endif  // CONFIGFILE_H
