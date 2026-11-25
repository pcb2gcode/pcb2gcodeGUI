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

// The pcb2gcode in $PCB2GCODE either describes its own options (--dump-options), which must
// then be readable by this GUI, or the GUI uses its bundled list
// (resources/pcb2gcode-options.json, pcb2gcode 3.2.0), which must then match that pcb2gcode.

#include "optionschema.h"
#include "pcb2gcode.h"
#include "units.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// Boost prints unit defaults in SI units: "2.54e-06 m", "0.0211667 m s^-1", "0.001 s", "150 %".
std::optional<double> siDefault(const QString &text, units::Dimension dim)
{
    if (text == "inf")
        return std::numeric_limits<double>::infinity();
    static const QRegularExpression re(QStringLiteral(R"(^(-?[0-9.e+-]+)\s*(m s\^-1|m|s|%)?$)"));
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch())
        return std::nullopt;
    const double v = m.captured(1).toDouble();
    const QString unit = m.captured(2);
    if (unit.isEmpty())
        return v == 0 ? std::optional<double>(0) : std::nullopt;  // bare: only 0 is unit-free
    if (unit == "m" && dim == units::Dimension::Length)
        return v * 1000;
    if (unit == "m s^-1" && dim == units::Dimension::Velocity)
        return v * 60000;
    if (unit == "s" && dim == units::Dimension::Time)
        return v;
    if (unit == "%" && dim == units::Dimension::Percent)
        return v;
    return std::nullopt;
}

bool sameDefault(const OptionSpec &spec, const HelpOption &help, QString *why)
{
    const QString printed = help.defaultText;
    if (spec.type == OptionType::Bool) {
        const bool ours = spec.defaultValue == "true";
        if ((printed == "1") != ours) {
            *why = QStringLiteral("bool default %1 vs %2").arg(spec.defaultValue, printed);
            return false;
        }
        return true;
    }
    if (spec.type == OptionType::Enum) {
        // Enums without an output operator print as numbers.
        if (!printed.isEmpty() && printed.at(0).isDigit())
            return true;
        if (spec.canonicalChoice(printed) != spec.canonicalChoice(spec.defaultValue)) {
            *why = QStringLiteral("default %1 vs %2").arg(spec.defaultValue, printed);
            return false;
        }
        return true;
    }
    const units::Dimension dim = spec.dimension();
    if (dim != units::Dimension::None && spec.type != OptionType::Double &&
        spec.type != OptionType::LengthOrPercent) {
        const auto theirs = siDefault(printed, dim);
        std::optional<double> ours;
        if (spec.defaultIsInfinite())
            ours = std::numeric_limits<double>::infinity();
        else if (const auto q = units::parse(spec.defaultValue, dim, true))
            ours = q->canonical;
        if (!theirs || !ours ||
            (*theirs != *ours && std::abs(*theirs - *ours) > 1e-4 * std::max(1.0, std::abs(*ours)))) {
            *why = QStringLiteral("default %1 vs printed %2").arg(spec.defaultValue, printed);
            return false;
        }
        return true;
    }
    if (spec.type == OptionType::LengthOrPercent) {
        const auto theirs = siDefault(printed, units::Dimension::Percent);
        const auto ours = units::parse(spec.defaultValue, units::Dimension::Percent, true);
        if (!theirs || !ours || std::abs(*theirs - ours->canonical) > 1e-6) {
            *why = QStringLiteral("default %1 vs printed %2").arg(spec.defaultValue, printed);
            return false;
        }
        return true;
    }
    if (printed != spec.defaultValue) {
        *why = QStringLiteral("default \"%1\" vs printed \"%2\"").arg(spec.defaultValue, printed);
        return false;
    }
    return true;
}

}  // namespace

class TestSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void snapshotMatchesHelp();
    void dumpIsReadable();

private:
    QString program;
    QString version;
    QByteArray dump;  // empty when this pcb2gcode can't describe its options
};

void TestSync::initTestCase()
{
    program = qEnvironmentVariable("PCB2GCODE");
    if (program.isEmpty())
        QSKIP("No pcb2gcode configured (set PCB2GCODE_EXECUTABLE when configuring)");
    QByteArray out;
    int code = -1;
    QVERIFY2(pcb2gcode::run(program, {"--version"}, 10000, &out, &code) && code == 0, qPrintable(program));
    version = QString::fromLocal8Bit(out).section('\n', 0, 0).trimmed();
    qInfo("pcb2gcode %s at %s", qPrintable(version), qPrintable(program));
    if (pcb2gcode::run(program, {QString::fromLatin1(pcb2gcode::kDumpOptionsFlag), "--noconfigfile"}, 10000, &out,
                       &code) &&
        code == 0)
        dump = out;
}

void TestSync::snapshotMatchesHelp()
{
    if (!dump.isEmpty())
        QSKIP("This pcb2gcode describes its own options, so the GUI doesn't use the bundled list with it");
    QByteArray out;
    int code = -1;
    QVERIFY(pcb2gcode::run(program, {"--help"}, 10000, &out, &code) && code == 0);
    const QList<HelpOption> help = OptionSchema::parseHelp(QString::fromLocal8Bit(out));
    QVERIFY(!help.isEmpty());
    const OptionSchema schema = OptionSchema::bundled();

    QHash<QString, QString> groupTitles;
    for (const OptionGroup &g : schema.groups)
        groupTitles.insert(g.id, g.title);

    QSet<QString> helpNames;
    QStringList problems;
    for (const HelpOption &h : help) {
        helpNames.insert(h.name);
        const OptionSpec *spec = schema.find(h.name);
        if (!spec) {
            problems << QStringLiteral("pcb2gcode has --%1, the bundled list doesn't").arg(h.name);
            continue;
        }
        if (groupTitles.value(spec->group) != h.group)
            problems << QStringLiteral("--%1 is in \"%2\", the list says \"%3\"")
                            .arg(h.name, h.group, groupTitles.value(spec->group));
        if (h.deprecated != spec->deprecated)
            problems << QStringLiteral("--%1: deprecated %2 vs %3").arg(h.name).arg(h.deprecated).arg(spec->deprecated);
        if (h.takesValue == (spec->type == OptionType::None))
            problems << QStringLiteral("--%1: takes a value or not").arg(h.name);
        if (h.hasImplicit != spec->hasImplicit)
            problems << QStringLiteral("--%1: implicit value differs").arg(h.name);
        if (h.hasDefault != spec->hasDefault && !(spec->hasDefault && spec->defaultValue.isEmpty()))
            problems << QStringLiteral("--%1: has a default or not").arg(h.name);
        QString why;
        if (h.hasDefault && spec->hasDefault && !sameDefault(*spec, h, &why))
            problems << QStringLiteral("--%1: %2").arg(h.name, why);
        QString theirs = h.description, ours = spec->description;
        theirs.remove(QRegularExpression(QStringLiteral("\\s")));
        ours.remove(QRegularExpression(QStringLiteral("\\s")));
        if (theirs != ours)
            problems << QStringLiteral("--%1: the description changed to \"%2\"").arg(h.name, h.description);
    }
    for (const OptionSpec &spec : schema.options)
        if (!helpNames.contains(spec.name))
            problems << QStringLiteral("the bundled list has --%1, pcb2gcode %2 doesn't").arg(spec.name, version);

    QVERIFY2(problems.isEmpty(),
             qPrintable("The bundled option list, which describes the pcb2gcode 3.2.0 release, doesn't match "
                        "this pcb2gcode (regenerate it with tools/update-option-snapshot.sh only if 3.2.0 "
                        "itself differs):\n  " +
                        problems.join("\n  ")));
}

void TestSync::dumpIsReadable()
{
    if (dump.isEmpty())
        QSKIP("This pcb2gcode has no option dump");
    QString error;
    const OptionSchema schema = OptionSchema::fromJson(dump, &error);
    QVERIFY2(schema.isValid(), qPrintable(error));

    // Everything this GUI would have to guess about.
    QStringList problems;
    condition::Context nothing;
    nothing.value = [](const QString &) { return std::optional<QString>(); };
    nothing.given = [](const QString &) { return false; };
    auto readable = [&](const QJsonValue &expression, const QString &where) {
        bool unknown = false;
        condition::evaluate(expression, nothing, true, &unknown);
        if (unknown)
            problems << QStringLiteral("%1 has an expression this GUI doesn't know").arg(where);
        for (const QString &name : condition::options(expression))
            if (!schema.find(name))
                problems << QStringLiteral("%1 refers to unknown option --%2").arg(where, name);
    };
    for (auto it = schema.conditions.begin(); it != schema.conditions.end(); ++it)
        readable(it->expression, "condition " + it.key());

    for (const QJsonValue &v : QJsonDocument::fromJson(dump).object().value("options").toArray()) {
        const QJsonObject o = v.toObject();
        const QString name = o.value("name").toString();
        const OptionSpec *spec = schema.find(name);
        if (optionTypeName(spec->type) != o.value("type").toString())
            problems << QStringLiteral("--%1 has type %2, which this GUI edits as text")
                            .arg(name, o.value("type").toString());
        for (const QString &when : {spec->usedWhen, spec->requiredWhen})
            if (!when.isEmpty() && when != "always" && !schema.conditions.contains(when))
                problems << QStringLiteral("--%1 uses unknown condition %2").arg(name, when);
    }
    const QStringList kinds = {"required", "range", "greater", "greater_or_equal", "forbidden"};
    for (const OptionRule &rule : schema.rules) {
        if (!kinds.contains(rule.kind))
            problems << QStringLiteral("rule \"%1\" has unknown kind %2").arg(rule.message, rule.kind);
        if (rule.when != "always" && !schema.conditions.contains(rule.when))
            problems << QStringLiteral("rule \"%1\" uses unknown condition %2").arg(rule.message, rule.when);
        for (const QJsonValue &e : {rule.unless, rule.condition})
            if (!e.isUndefined() && !e.isNull())
                readable(e, "rule \"" + rule.message + "\"");
        for (const QString &name : rule.options)
            if (!schema.find(name))
                problems << QStringLiteral("rule \"%1\" names unknown option --%2").arg(rule.message, name);
    }
    QVERIFY2(problems.isEmpty(), qPrintable("pcb2gcode's option dump has parts this GUI can't read:\n  " +
                                            problems.join("\n  ")));
}

QTEST_GUILESS_MAIN(TestSync)
#include "tst_sync.moc"
