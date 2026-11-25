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

#ifndef LAYOUT_H
#define LAYOUT_H

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

// The GUI's presentation of the options: tabs, sections, labels, starting values and
// "enabled when" rules. Everything about the options themselves comes from OptionSchema;
// options missing here are still shown, in the tab of their group.

struct OptionHint {
    QString label;
    QString preset;     // starting value instead of pcb2gcode's default, in pcb2gcode syntax
    QString enabledIf;  // condition
    QString placeholder;
    QMap<QString, QString> choiceLabels;  // shown text per value (enums, or bools as a combo)
    bool perSecond = false;  // show velocities per second
    bool fine = false;       // small spin box steps
};

struct LayoutSection {
    QString title;
    QStringList options;
};

struct LayoutTab {
    QString id;
    QString title;
    QString wiki;  // page of the pcb2gcode wiki
    QList<LayoutSection> sections;
};

struct Layout {
    QStringList hidden;
    QMap<QString, QString> groupTabs;  // option group id -> tab id
    QList<LayoutTab> tabs;
    QMap<QString, OptionHint> hints;

    static Layout fromJson(const QByteArray &json, QString *error = nullptr);
    static Layout bundled();

    OptionHint hint(const QString &option) const { return hints.value(option); }
    QStringList placedOptions() const;
};

#endif  // LAYOUT_H
