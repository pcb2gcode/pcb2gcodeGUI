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
        {"double", OptionType::Double},
        {"string", OptionType::String},
        {"enum", OptionType::Enum},
        {"length", OptionType::Length},
        {"velocity", OptionType::Velocity},
        {"rpm", OptionType::Rpm},
        {"time", OptionType::Time},
        {"percent", OptionType::Percent},
        {"length_or_percent", OptionType::LengthOrPercent},
        {"available_drill", OptionType::Drill},
    };
    return names;
}

PathKind pathKind(const QString &path, const QString &format)
{
    if (path == "directory")
        return PathKind::Directory;
    if (path == "output_file")
        return PathKind::OutputFile;
    if (path != "input_file")
        return PathKind::None;
    if (format == "gerber")
        return PathKind::InputGerber;
    if (format == "excellon")
        return PathKind::InputExcellon;
    if (format == "gcode")
        return PathKind::InputGcode;
    if (format == "text")
        return PathKind::InputText;
    return PathKind::InputFile;
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

void readRange(const QJsonObject &o, QString *min, QString *max, bool *minExclusive, bool *maxExclusive)
{
    *min = valueText(o.value("min"));
    *max = valueText(o.value("max"));
    *minExclusive = o.value("min_exclusive").toBool();
    *maxExclusive = o.value("max_exclusive").toBool();
}

// The unit words of the dump's "types", in the GUI's canonical units (mm, minutes).
std::optional<units::Grammar> readGrammar(const QJsonObject &types)
{
    if (types.isEmpty())
        return std::nullopt;
    units::Grammar g = units::builtInGrammar();
    auto words = [](const QJsonValue &unit) {
        QStringList w{unit.toObject().value("name").toString()};
        for (const QJsonValue &a : unit.toObject().value("aliases").toArray())
            w.append(a.toString());
        return w;
    };
    const QJsonArray lengths = types.value("length").toObject().value("units").toArray();
    if (!lengths.isEmpty()) {
        g.lengthMm.clear();
        for (const QJsonValue &u : lengths)
            for (const QString &w : words(u))
                g.lengthMm.insert(w, u.toObject().value("factor").toDouble() * 1000);
    }
    const QJsonArray times = types.value("time").toObject().value("units").toArray();
    if (!times.isEmpty()) {
        g.timeMinutes.clear();
        for (const QJsonValue &u : times)
            for (const QString &w : words(u))
                g.timeMinutes.insert(w, u.toObject().value("factor").toDouble() / 60);
    }
    const QJsonArray revolutions = types.value("revolution").toObject().value("units").toArray();
    if (!revolutions.isEmpty()) {
        g.revolutions.clear();
        for (const QJsonValue &u : revolutions)
            g.revolutions += words(u);
    }
    const QJsonArray rpm = types.value("rpm").toObject().value("units").toArray();
    if (!rpm.isEmpty()) {
        g.rpm.clear();
        for (const QJsonValue &u : rpm)
            g.rpm += words(u);
    }
    return g;
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
    if (!root.value("format_version").isDouble() || !root.value("options").isArray()) {
        if (error)
            *error = QStringLiteral("not pcb2gcode option metadata");
        return schema;
    }
    schema.formatVersion = root.value("format_version").toInt();
    if (schema.formatVersion != 1) {
        if (error)
            *error = QStringLiteral("unsupported format_version %1").arg(schema.formatVersion);
        return schema;
    }
    schema.pcb2gcodeVersion = root.value("version").toString();
    schema.gitCommit = root.value("git_commit").toString();
    schema.sourceCommit = root.value("source_commit").toString();
    schema.grammar = readGrammar(root.value("types").toObject());

    const QJsonObject conditions = root.value("conditions").toObject();
    for (auto it = conditions.begin(); it != conditions.end(); ++it) {
        const QJsonObject c = it.value().toObject();
        schema.conditions.insert(it.key(), {c.value("description").toString(), c.value("expression")});
    }

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
        spec.shortName = o.value("short").toString();
        spec.group = o.value("group").toString();
        spec.description = o.value("description").toString();
        // Options of types this version doesn't know are edited as text.
        spec.type = typeNames().value(o.value("type").toString(), OptionType::String);
        if (o.value("comma_separated").toBool())
            spec.multiplicity = Multiplicity::CommaList;
        else if (o.value("repeatable").toBool())
            spec.multiplicity = Multiplicity::Repeat;
        const QJsonValue def = o.value("default");
        spec.hasDefault = !def.isUndefined() && !def.isNull();
        if (def.isArray()) {
            QStringList items;
            for (const QJsonValue &item : def.toArray())
                items.append(valueText(item));
            spec.defaultValue = items.join(spec.multiplicity == Multiplicity::Repeat ? "\n" : ",");
        } else if (spec.hasDefault) {
            spec.defaultValue = valueText(def);
        }
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
        readRange(o.value("range").toObject(), &spec.minimum, &spec.maximum, &spec.minimumExclusive,
                  &spec.maximumExclusive);
        spec.usedWhen = o.value("used_when").toString(QStringLiteral("always"));
        spec.requiredWhen = o.value("required_when").toString();
        spec.example = valueText(o.value("example"));
        spec.deprecated = o.value("deprecated").toBool();
        spec.replacedBy = o.value("replaced_by").toString();
        const QJsonObject valueMap = o.value("value_map").toObject();
        for (auto it = valueMap.begin(); it != valueMap.end(); ++it)
            spec.valueMap.insert(it.key(), valueText(it.value()));
        spec.cliOnly = o.value("cli_only").toBool();
        spec.path = pathKind(o.value("path").toString(), o.value("file_format").toString());
        spec.bareLength = !o.value("bare_unit").toString().isEmpty();
        schema.options.append(spec);
    }

    for (const QJsonValue &r : root.value("rules").toArray()) {
        const QJsonObject o = r.toObject();
        OptionRule rule;
        rule.kind = o.value("kind").toString();
        for (const QJsonValue &name : o.value("options").toArray())
            rule.options.append(name.toString());
        rule.when = o.value("when").toString(QStringLiteral("always"));
        rule.code = o.value("code").toInt();
        rule.message = o.value("message").toString();
        readRange(o, &rule.minimum, &rule.maximum, &rule.minimumExclusive, &rule.maximumExclusive);
        rule.unless = o.value("unless");
        rule.condition = o.value("if");
        schema.rules.append(rule);
    }

    for (const QJsonValue &c : root.value("exit_codes").toArray())
        schema.exitCodes.insert(c.toObject().value("code").toInt(), c.toObject().value("name").toString());

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

bool OptionSchema::holds(const QString &conditionName, const condition::Context &context, bool *sawUnknown) const
{
    if (conditionName.isEmpty() || conditionName == "always")
        return true;
    const auto it = conditions.constFind(conditionName);
    if (it == conditions.constEnd()) {
        if (sawUnknown)
            *sawUnknown = true;
        return true;
    }
    return condition::evaluate(it->expression, context, true, sawUnknown);
}

QString OptionSchema::conditionDescription(const QString &conditionName) const
{
    return conditions.value(conditionName).description;
}

QString OptionSchema::conditionClause(const QString &conditionName) const
{
    QString clause = conditionDescription(conditionName).trimmed();
    while (clause.endsWith('.'))
        clause.chop(1);
    // Keep acronyms and names like "LinuxCNC" as they are.
    if (clause.size() > 1 && clause.at(0).isUpper() && !clause.at(1).isUpper())
        clause[0] = clause.at(0).toLower();
    return clause;
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
