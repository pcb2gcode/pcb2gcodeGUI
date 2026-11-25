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

#ifndef UNITS_H
#define UNITS_H

#include <QHash>
#include <QString>
#include <QStringList>
#include <optional>

// The unit grammar of pcb2gcode's units.hpp: a number, optional whitespace and an
// optional unit. Values are converted to one canonical unit per dimension:
// mm, mm/min, rpm, s and %.
namespace units {

enum class Dimension { None, Length, Velocity, Rpm, Time, Percent };

struct Quantity {
    double value = 0;      // the number as written
    QString unit;          // the unit as written, empty for a bare number
    double canonical = 0;  // the value in the dimension's canonical unit
};

// The unit words pcb2gcode accepts. pcb2gcode's option dump lists them; without it the
// GUI uses those of pcb2gcode 3.2.0.
struct Grammar {
    QHash<QString, double> lengthMm;     // unit word -> millimetres
    QHash<QString, double> timeMinutes;  // unit word -> minutes
    QStringList revolutions;             // words for one revolution
    QStringList rpm;                     // words for revolutions per minute
};

Grammar builtInGrammar();
void setGrammar(const Grammar &grammar);

// Parses `text` the way pcb2gcode does. A bare number is read like pcb2gcode reads it:
// inches (or mm with --metric) for lengths, in/min (or mm/min) for velocities,
// milliseconds for times and a fraction for percentages.
std::optional<Quantity> parse(const QString &text, Dimension dimension, bool metric,
                              QString *error = nullptr);

// Formats a number for pcb2gcode: '.' as decimal separator, no exponent, no trailing zeros.
QString formatNumber(double value, int maxDecimals = 6);

// The unit the GUI shows and emits for a dimension in the chosen unit system.
QString displayUnit(Dimension dimension, bool metric, bool perSecond = false);
double toDisplay(double canonical, Dimension dimension, bool metric, bool perSecond = false);
double fromDisplay(double display, Dimension dimension, bool metric, bool perSecond = false);

// Whether a dimension changes when switching between millimetres and inches.
bool followsUnitSystem(Dimension dimension);

}  // namespace units

#endif  // UNITS_H
