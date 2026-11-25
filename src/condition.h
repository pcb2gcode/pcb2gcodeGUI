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

#ifndef CONDITION_H
#define CONDITION_H

#include "units.h"

#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>

// Conditions over option values, such as "front || back" or
// "drill && min-milldrill-hole-diameter < inf".
//
// An option name on its own is true when the option has a value that isn't false, empty,
// zero or infinite. Comparisons (== != < <= > >=) take option names, numbers with optional
// units, quoted strings and the words true, false and inf. A word that isn't an option
// name is a string, so "software == custom" works; quote words that are also option names.
namespace condition {

enum class Kind { Unknown, Bool, Text, Number };

struct Context {
    std::function<bool(const QString &name)> isOption;
    // The effective value (explicit or default) in pcb2gcode syntax, or nullopt if unset.
    std::function<std::optional<QString>(const QString &name)> value;
    // Whether the option is passed to pcb2gcode, as opposed to left at its default.
    std::function<bool(const QString &name)> given;
    std::function<Kind(const QString &name)> kind;
    std::function<units::Dimension(const QString &name)> dimension;
    bool metric = false;
};

// Returns the value of `expression`; an empty expression is true. On a syntax error,
// returns `fallback` and sets `error`.
bool evaluate(const QString &expression, const Context &context, bool fallback = true,
              QString *error = nullptr);

// The words in `expression` that could be option names.
QStringList identifiers(const QString &expression);

// Evaluates an expression of pcb2gcode's option dump: true, {"set": o}, {"given": o},
// {"equal": [o, literal]}, {"finite": o}, {"not": e}, {"any": [...]}, {"all": [...]}.
// Nodes this version doesn't know count as `unknownValue` and set `*sawUnknown`.
bool evaluate(const QJsonValue &expression, const Context &context, bool unknownValue,
              bool *sawUnknown = nullptr);

// The options an expression of the option dump refers to.
QStringList options(const QJsonValue &expression);

// Compares two values of an option's dimension; nullopt when one doesn't parse.
std::optional<int> compareQuantities(const QString &a, const QString &b, units::Dimension dimension,
                                     bool metric);

}  // namespace condition

#endif  // CONDITION_H
