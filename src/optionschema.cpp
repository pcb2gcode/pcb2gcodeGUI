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

#include "optionschema.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {

const QHash<QString, OptionType> &typeNames()
{
    static const QHash<QString, OptionType> names = {
        {"none", OptionType::None},
        {"bool", OptionType::Bool},
        {"int", OptionType::Int},
        {"uint", OptionType::UInt},
        {"size", OptionType::Size},
        {"double", OptionType::Double},
        {"string", OptionType::String},
        {"enum", OptionType::Enum},
        {"length", OptionType::Length},
        {"velocity", OptionType::Velocity},
        {"rpm", OptionType::Rpm},
        {"time", OptionType::Time},
        {"percent", OptionType::Percent},
        {"length_or_percent", OptionType::LengthOrPercent},
        {"drill", OptionType::Drill},
    };
    return names;
}

PathKind pathKind(const QString &s)
{
    static const QHash<QString, PathKind> kinds = {
        {"input-gerber", PathKind::InputGerber},
        {"input-excellon", PathKind::InputExcellon},
        {"input-gcode", PathKind::InputGcode},
        {"input-text", PathKind::InputText},
        {"output-file", PathKind::OutputFile},
        {"directory", PathKind::Directory},
    };
    return kinds.value(s, PathKind::None);
}

// JSON values may be strings, numbers or booleans; the GUI works with pcb2gcode syntax.
QString valueText(const QJsonValue &v)
{
    if (v.isString())
        return v.toString();
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble())
        return units::formatNumber(v.toDouble(), 10);
    return QString();
}

}  // namespace

QString optionTypeName(OptionType type)
{
    return typeNames().key(type, QStringLiteral("string"));
}

units::Dimension OptionSpec::dimension() const
{
    switch (type) {
    case OptionType::Length:
    case OptionType::Drill:
        return units::Dimension::Length;
    case OptionType::Velocity:
        return units::Dimension::Velocity;
    case OptionType::Rpm:
        return units::Dimension::Rpm;
    case OptionType::Time:
        return units::Dimension::Time;
    case OptionType::Percent:
        return units::Dimension::Percent;
    case OptionType::Double:
        return bareLength ? units::Dimension::Length : units::Dimension::None;
    default:
        return units::Dimension::None;
    }
}

bool OptionSpec::isNumeric() const
{
    switch (type) {
    case OptionType::Int:
    case OptionType::UInt:
    case OptionType::Size:
    case OptionType::Double:
    case OptionType::Length:
    case OptionType::Velocity:
    case OptionType::Rpm:
    case OptionType::Time:
    case OptionType::Percent:
        return true;
    default:
        return false;
    }
}

bool OptionSpec::defaultIsInfinite() const
{
    return hasDefault && defaultValue.trimmed().compare("inf", Qt::CaseInsensitive) == 0;
}

QString OptionSpec::canonicalChoice(const QString &value) const
{
    const QString v = value.trimmed();
    for (const OptionChoice &c : choices) {
        if (c.value.compare(v, Qt::CaseInsensitive) == 0)
            return c.value;
        for (const QString &alias : c.aliases)
            if (alias.compare(v, Qt::CaseInsensitive) == 0)
                return c.value;
    }
    return QString();
}

OptionSchema OptionSchema::fromJson(const QByteArray &json, QString *error)
{
    OptionSchema schema;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("not a JSON object: %1").arg(parseError.errorString());
        return schema;
    }
    const QJsonObject root = doc.object();
    if (!root.value("options").isArray()) {
        if (error)
            *error = QStringLiteral("no \"options\" list");
        return schema;
    }
    schema.formatVersion = root.value("format_version").toInt(1);
    if (schema.formatVersion != 1) {
        if (error)
            *error = QStringLiteral("unsupported format_version %1").arg(schema.formatVersion);
        return schema;
    }
    schema.pcb2gcodeVersion = root.value("pcb2gcode_version").toString();
    schema.pcb2gcodeCommit = root.value("pcb2gcode_commit").toString();

    for (const QJsonValue &g : root.value("groups").toArray()) {
        const QJsonObject o = g.toObject();
        schema.groups.append({o.value("id").toString(), o.value("title").toString()});
    }

    for (const QJsonValue &value : root.value("options").toArray()) {
        const QJsonObject o = value.toObject();
        OptionSpec spec;
        spec.name = o.value("name").toString();
        if (spec.name.isEmpty())
            continue;
        spec.shortName = o.value("short_name").toString();
        spec.group = o.value("group").toString();
        spec.description = o.value("description").toString();
        spec.type = typeNames().value(o.value("type").toString(), OptionType::String);
        const QString multiple = o.value("multiple").toString();
        if (multiple == "comma")
            spec.multiplicity = Multiplicity::CommaList;
        else if (multiple == "repeat")
            spec.multiplicity = Multiplicity::Repeat;
        const QJsonValue def = o.value("default");
        spec.hasDefault = !def.isUndefined() && !def.isNull();
        if (spec.hasDefault)
            spec.defaultValue = valueText(def);
        spec.defaultNote = o.value("default_note").toString();
        const QJsonValue implicit = o.value("implicit");
        spec.hasImplicit = !implicit.isUndefined() && !implicit.isNull();
        if (spec.hasImplicit)
            spec.implicitValue = valueText(implicit);
        for (const QJsonValue &c : o.value("choices").toArray()) {
            OptionChoice choice;
            if (c.isString()) {
                choice.value = c.toString();
            } else {
                choice.value = c.toObject().value("value").toString();
                for (const QJsonValue &a : c.toObject().value("aliases").toArray())
                    choice.aliases.append(a.toString());
            }
            spec.choices.append(choice);
        }
        spec.minimum = valueText(o.value("min"));
        spec.maximum = valueText(o.value("max"));
        spec.minimumExclusive = o.value("min_exclusive").toBool();
        spec.maximumExclusive = o.value("max_exclusive").toBool();
        spec.usedWhen = o.value("used_when").toString();
        spec.requiredWhen = o.value("required_when").toString();
        spec.example = valueText(o.value("example"));
        spec.deprecated = o.value("deprecated").toBool();
        spec.replacedBy = o.value("replaced_by").toString();
        spec.cliOnly = o.value("cli_only").toBool();
        spec.path = pathKind(o.value("path").toString());
        spec.bareLength = o.value("bare_unit").toString() == "length";
        schema.options.append(spec);
    }

    for (const QJsonValue &c : root.value("constraints").toArray()) {
        const QJsonObject o = c.toObject();
        schema.constraints.append(
            {o.value("when").toString(), o.value("require").toString(), o.value("message").toString()});
    }

    if (schema.options.isEmpty() && error)
        *error = QStringLiteral("the option list is empty");
    return schema;
}

OptionSchema OptionSchema::bundled()
{
    QFile file(QStringLiteral(":/pcb2gcode-options.json"));
    if (!file.open(QIODevice::ReadOnly))
        return OptionSchema();
    OptionSchema schema = fromJson(file.readAll());
    schema.source = Source::Bundled;
    return schema;
}

QList<HelpOption> OptionSchema::parseHelp(const QString &helpText)
{
    // Boost.Program_options layout:
    //   Section title:
    //     --name arg (=default)    description
    //     -V [ --version ]         description
    //     --flag [=arg(=1)] (=0)   description
    //                              continued description
    static const QRegularExpression optionLine(
        QStringLiteral(R"(^  (?:-(\S) \[ )?--([A-Za-z0-9][A-Za-z0-9_-]*)(?: \])?(.*)$)"));
    static const QRegularExpression implicitRe(QStringLiteral(R"(^\s*\[=arg\(=[^)]*\)\])"));
    static const QRegularExpression argRe(QStringLiteral(R"(^\s*arg\b)"));
    // Defaults may contain parentheses and spaces, so prefer a ')' followed by the column gap.
    static const QRegularExpression defaultRe(QStringLiteral(R"(^\s*\(=(.*?)\)(?=\s{2,}|\s*$))"));
    static const QRegularExpression shortDefaultRe(QStringLiteral(R"(^\s*\(=(.*?)\)(?=\s))"));

    QList<HelpOption> result;
    QString group;
    const QStringList lines = helpText.split('\n');
    for (const QString &rawLine : lines) {
        const QString line = rawLine.endsWith('\r') ? rawLine.chopped(1) : rawLine;
        if (line.trimmed().isEmpty())
            continue;
        if (!line.startsWith(' ') && line.endsWith(':')) {
            group = line.chopped(1);
            continue;
        }
        const QRegularExpressionMatch m = optionLine.match(line);
        if (m.hasMatch()) {
            HelpOption opt;
            opt.shortName = m.captured(1);
            opt.name = m.captured(2);
            opt.group = group;
            QString rest = m.captured(3);
            const auto im = implicitRe.match(rest);
            if (im.hasMatch()) {
                opt.takesValue = true;
                opt.hasImplicit = true;
                rest = rest.mid(im.capturedLength());
            } else {
                const auto am = argRe.match(rest);
                if (am.hasMatch()) {
                    opt.takesValue = true;
                    rest = rest.mid(am.capturedLength());
                }
            }
            auto dm = defaultRe.match(rest);
            if (!dm.hasMatch())
                dm = shortDefaultRe.match(rest);
            if (dm.hasMatch()) {
                opt.hasDefault = true;
                opt.defaultText = dm.captured(1);
                rest = rest.mid(dm.capturedLength());
            }
            opt.description = rest.trimmed();
            result.append(opt);
        } else if (!result.isEmpty() && line.startsWith("    ")) {
            QString &d = result.last().description;
            const QString more = line.trimmed();
            if (d.endsWith('-') && !more.isEmpty() && more.at(0).isLower())
                d += more;  // Boost breaks long words at the column limit
            else
                d += (d.isEmpty() ? QString() : QStringLiteral(" ")) + more;
        }
    }
    for (HelpOption &opt : result)
        opt.deprecated = opt.description.contains(QStringLiteral("[DEPRECATED"));
    return result;
}

void OptionSchema::restrictTo(const QList<HelpOption> &help)
{
    QSet<QString> names;
    for (const HelpOption &h : help)
        names.insert(h.name);

    missingFromBinary.clear();
    for (OptionSpec &spec : options) {
        spec.unsupported = !names.contains(spec.name);
        if (spec.unsupported)
            missingFromBinary.append(spec.name);
    }

    addedFromHelp.clear();
    for (const HelpOption &h : help) {
        if (find(h.name))
            continue;
        OptionSpec spec;
        spec.name = h.name;
        spec.shortName = h.shortName;
        spec.group = QStringLiteral("other");
        spec.description = h.description;
        spec.deprecated = h.deprecated;
        spec.fromHelp = true;
        if (!h.takesValue) {
            spec.type = OptionType::None;
        } else if (h.hasImplicit && (h.defaultText == "0" || h.defaultText == "1")) {
            spec.type = OptionType::Bool;
            spec.hasDefault = true;
            spec.defaultValue = h.defaultText == "1" ? QStringLiteral("true") : QStringLiteral("false");
        } else {
            // The printed default may be in SI units, so it can't be fed back to pcb2gcode.
            spec.type = OptionType::String;
        }
        options.append(spec);
        addedFromHelp.append(h.name);
    }
    if (!addedFromHelp.isEmpty()) {
        bool haveOther = false;
        for (const OptionGroup &g : groups)
            haveOther = haveOther || g.id == "other";
        if (!haveOther)
            groups.append({QStringLiteral("other"), QStringLiteral("Other options")});
    }
}

const OptionSpec *OptionSchema::find(const QString &name) const
{
    for (const OptionSpec &spec : options)
        if (spec.name == name)
            return &spec;
    return nullptr;
}

OptionSpec *OptionSchema::find(const QString &name)
{
    for (OptionSpec &spec : options)
        if (spec.name == name)
            return &spec;
    return nullptr;
}
