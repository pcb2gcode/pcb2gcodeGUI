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

#ifndef OPTIONSCHEMA_H
#define OPTIONSCHEMA_H

#include "units.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// pcb2gcode's options as described by pcb2gcode itself (its option dump), or by the
// snapshot of that dump bundled with the GUI.

enum class OptionType {
    None,  // a switch without a value, such as --help
    Bool,
    Int,
    UInt,
    Size,
    Double,
    String,
    Enum,
    Length,
    Velocity,
    Rpm,
    Time,
    Percent,
    LengthOrPercent,
    Drill,  // diameter[:negative tolerance[:+positive tolerance]]
};

enum class Multiplicity { Single, CommaList, Repeat };

enum class PathKind { None, InputGerber, InputExcellon, InputGcode, InputText, OutputFile, Directory };

struct OptionChoice {
    QString value;
    QStringList aliases;
};

struct OptionSpec {
    QString name;
    QString shortName;
    QString group;
    QString description;
    OptionType type = OptionType::String;
    Multiplicity multiplicity = Multiplicity::Single;
    bool hasDefault = false;
    QString defaultValue;  // in pcb2gcode syntax; "inf" can't be typed, only left unset
    QString defaultNote;   // what an unset or special default means
    bool hasImplicit = false;
    QString implicitValue;
    QList<OptionChoice> choices;
    QString minimum, maximum;
    bool minimumExclusive = false, maximumExclusive = false;
    QString usedWhen;      // condition; empty means always
    QString requiredWhen;  // condition; empty means never
    QString example;
    bool deprecated = false;
    QString replacedBy;
    bool cliOnly = false;
    PathKind path = PathKind::None;
    bool bareLength = false;  // a plain number in --metric units, e.g. --tolerance
    bool fromHelp = false;    // only known from --help, nothing but the name is reliable
    bool unsupported = false; // in the snapshot but not in the installed pcb2gcode

    units::Dimension dimension() const;
    bool isNumeric() const;
    bool defaultIsInfinite() const;
    // The canonical choice for a value or alias, or an empty string.
    QString canonicalChoice(const QString &value) const;
};

struct OptionGroup {
    QString id;
    QString title;
};

struct OptionConstraint {
    QString when;
    QString require;
    QString message;
};

// One option as printed by `pcb2gcode --help`.
struct HelpOption {
    QString name;
    QString shortName;
    QString group;  // the section title
    QString description;
    bool takesValue = false;
    bool hasImplicit = false;
    QString defaultText;  // as printed, e.g. "0", "auto", "2.54e-06 m"
    bool hasDefault = false;
    bool deprecated = false;
};

class OptionSchema
{
public:
    enum class Source { None, Dump, Bundled };

    static OptionSchema fromJson(const QByteArray &json, QString *error = nullptr);
    static OptionSchema bundled();
    static QList<HelpOption> parseHelp(const QString &helpText);

    // Hides options the installed pcb2gcode doesn't have and adds the ones it has that the
    // schema doesn't describe. Used when the schema doesn't come from the installed binary.
    void restrictTo(const QList<HelpOption> &help);

    const OptionSpec *find(const QString &name) const;
    OptionSpec *find(const QString &name);

    bool isValid() const { return !options.isEmpty(); }

    Source source = Source::None;
    int formatVersion = 0;
    QString pcb2gcodeVersion;
    QString pcb2gcodeCommit;
    QList<OptionGroup> groups;
    QList<OptionSpec> options;
    QList<OptionConstraint> constraints;
    QStringList addedFromHelp;  // names added by restrictTo()
    QStringList missingFromBinary;
};

QString optionTypeName(OptionType type);

#endif  // OPTIONSCHEMA_H
