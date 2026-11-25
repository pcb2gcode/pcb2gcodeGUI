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

#include "condition.h"
#include "configfile.h"
#include "layout.h"
#include "optionschema.h"
#include "pcb2gcode.h"
#include "previews.h"
#include "units.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>

using units::Dimension;

namespace {

QString testData(const QString &name)
{
    return QDir(qEnvironmentVariable("PCB2GCODEGUI_TEST_DATA")).filePath(name);
}

QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

bool shownInForm(const OptionSpec &spec, const Layout &layout)
{
    return spec.type != OptionType::None && !spec.cliOnly && !spec.deprecated && !layout.hidden.contains(spec.name);
}

}  // namespace

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void parseUnits_data();
    void parseUnits();
    void rejectBadUnits_data();
    void rejectBadUnits();
    void formatNumbers();
    void conditions_data();
    void conditions();
    void bundledSchema();
    void dumpFormat();
    void dumpExpressions();
    void checkOutput();
    void layoutMatchesSchema();
    void parseHelp();
    void restrictToHelp();
    void parseConfig();
    void migrateLegacyConfig();
    void previewsByFileName();
    void previewsByManifest();
};

void TestCore::parseUnits_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("dimension");
    QTest::addColumn<bool>("metric");
    QTest::addColumn<double>("canonical");

    QTest::newRow("mm") << "-0.05mm" << int(Dimension::Length) << false << -0.05;
    QTest::newRow("inch word") << "-0.04inch" << int(Dimension::Length) << true << -1.016;
    QTest::newRow("bare inch") << "0.08" << int(Dimension::Length) << false << 2.032;
    QTest::newRow("bare mm") << "0.08" << int(Dimension::Length) << true << 0.08;
    QTest::newRow("mil") << "1 mil" << int(Dimension::Length) << true << 0.0254;
    QTest::newRow("in/min") << "360 in/min" << int(Dimension::Velocity) << true << 9144.0;
    QTest::newRow("mm/s") << "10mm/s" << int(Dimension::Velocity) << false << 600.0;
    QTest::newRow("per") << "100 mm per min" << int(Dimension::Velocity) << false << 100.0;
    QTest::newRow("bare in/min") << "30" << int(Dimension::Velocity) << false << 762.0;
    QTest::newRow("rotations/second") << "200rotations/second" << int(Dimension::Rpm) << true << 12000.0;
    QTest::newRow("bare rpm") << "12000" << int(Dimension::Rpm) << true << 12000.0;
    QTest::newRow("ms") << "1 ms" << int(Dimension::Time) << true << 0.001;
    QTest::newRow("s") << "2s" << int(Dimension::Time) << true << 2.0;
    QTest::newRow("minutes") << "1.5 min" << int(Dimension::Time) << true << 90.0;
    QTest::newRow("bare time is ms") << "1000" << int(Dimension::Time) << true << 1.0;
    QTest::newRow("percent") << "50%" << int(Dimension::Percent) << true << 50.0;
    QTest::newRow("bare percent is a fraction") << "0.5" << int(Dimension::Percent) << true << 50.0;
    QTest::newRow("exponent") << "4e-4" << int(Dimension::Length) << false << 0.01016;
    QTest::newRow("exponent with unit") << "4e-4mm" << int(Dimension::Length) << false << 0.0004;
    QTest::newRow("capital exponent") << "1.5E+2 mm/min" << int(Dimension::Velocity) << false << 150.0;
    QTest::newRow("plain exponent") << "2.5e3" << int(Dimension::None) << true << 2500.0;
}

void TestCore::parseUnits()
{
    QFETCH(QString, text);
    QFETCH(int, dimension);
    QFETCH(bool, metric);
    QFETCH(double, canonical);
    QString error;
    const auto q = units::parse(text, Dimension(dimension), metric, &error);
    QVERIFY2(q.has_value(), qPrintable(error));
    QVERIFY2(std::abs(q->canonical - canonical) < 1e-9 * std::max(1.0, std::abs(canonical)),
             qPrintable(QString::number(q->canonical, 'g', 12)));
}

void TestCore::rejectBadUnits_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("dimension");
    QTest::newRow("not a number") << "abc" << int(Dimension::Length);
    QTest::newRow("unknown unit") << "5 km" << int(Dimension::Length);
    QTest::newRow("pcb2gcode doesn't know meters") << "5 meters" << int(Dimension::Length);
    QTest::newRow("trailing text") << "5mm extra" << int(Dimension::Length);
    QTest::newRow("exponent without digits") << "2e" << int(Dimension::Length);
    QTest::newRow("unit starting with e") << "2em" << int(Dimension::Length);
    QTest::newRow("speed without time") << "5mm" << int(Dimension::Velocity);
    QTest::newRow("percent sign missing") << "5 pct" << int(Dimension::Percent);
    QTest::newRow("inf can't be typed") << "inf" << int(Dimension::Length);
}

void TestCore::rejectBadUnits()
{
    QFETCH(QString, text);
    QFETCH(int, dimension);
    QVERIFY(!units::parse(text, Dimension(dimension), true).has_value());
}

void TestCore::formatNumbers()
{
    QCOMPARE(units::formatNumber(0.1), QStringLiteral("0.1"));
    QCOMPARE(units::formatNumber(-0.0), QStringLiteral("0"));
    QCOMPARE(units::formatNumber(1.5), QStringLiteral("1.5"));
    QCOMPARE(units::formatNumber(2), QStringLiteral("2"));
    QCOMPARE(units::formatNumber(0.000254), QStringLiteral("0.000254"));
    QCOMPARE(units::formatNumber(-0.0019685, 5), QStringLiteral("-0.00197"));
    QCOMPARE(units::formatNumber(1234567.5), QStringLiteral("1234567.5"));
}

void TestCore::conditions_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<bool>("expected");
    QTest::newRow("empty") << "" << true;
    QTest::newRow("set file") << "front" << true;
    QTest::newRow("unset file") << "back" << false;
    QTest::newRow("or") << "front || back" << true;
    QTest::newRow("and") << "front && back" << false;
    QTest::newRow("not bool") << "!tsp-2opt" << false;
    QTest::newRow("word literal") << "software == custom" << true;
    QTest::newRow("quoted literal") << "software == 'linuxcnc'" << false;
    QTest::newRow("infinite default") << "drill && min-milldrill-hole-diameter < inf" << false;
    QTest::newRow("compare lengths") << "zsafe > zwork" << true;
    QTest::newRow("compare with unit") << "zsafe > 10mm" << false;
    QTest::newRow("compare inches") << "zsafe < 0.5in" << true;
    QTest::newRow("parentheses") << "(front && !back) || nope" << true;
    QTest::newRow("enum") << "mill-feed-direction != any" << true;
    QTest::newRow("false bool") << "zchange-absolute" << false;
    QTest::newRow("dashed digit names") << "nog91-1 || tsp-2opt" << true;
}

void TestCore::conditions()
{
    QFETCH(QString, expression);
    QFETCH(bool, expected);
    const QHash<QString, QString> values = {
        {"front", "board-F.gbr"},
        {"drill", "board.drl"},
        {"min-milldrill-hole-diameter", "inf"},
        {"tsp-2opt", "true"},
        {"software", "custom"},
        {"zsafe", "5mm"},
        {"zwork", "-0.05mm"},
        {"zchange-absolute", "false"},
        {"mill-feed-direction", "climb"},
        {"nog91-1", "false"},
    };
    const QSet<QString> bools = {"tsp-2opt", "zchange-absolute", "nog91-1"};
    const QSet<QString> lengths = {"min-milldrill-hole-diameter", "zsafe", "zwork"};
    condition::Context ctx;
    ctx.isOption = [&](const QString &n) { return values.contains(n) || n == "back"; };
    ctx.value = [&](const QString &n) -> std::optional<QString> {
        if (!values.contains(n))
            return std::nullopt;
        return values.value(n);
    };
    ctx.kind = [&](const QString &n) {
        if (bools.contains(n))
            return condition::Kind::Bool;
        if (lengths.contains(n))
            return condition::Kind::Number;
        return condition::Kind::Text;
    };
    ctx.dimension = [&](const QString &n) { return lengths.contains(n) ? Dimension::Length : Dimension::None; };
    ctx.metric = true;
    QString error;
    QCOMPARE(condition::evaluate(expression, ctx, !expected, &error), expected);
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

void TestCore::bundledSchema()
{
    const OptionSchema schema = OptionSchema::bundled();
    QVERIFY(schema.isValid());
    QCOMPARE(schema.source, OptionSchema::Source::Bundled);
    QVERIFY(!schema.pcb2gcodeVersion.isEmpty());

    QSet<QString> groups, names;
    for (const OptionGroup &g : schema.groups)
        groups.insert(g.id);
    for (const OptionSpec &spec : schema.options) {
        QVERIFY2(!names.contains(spec.name), qPrintable("duplicate " + spec.name));
        names.insert(spec.name);
        QVERIFY2(groups.contains(spec.group), qPrintable(spec.name + " has an unknown group"));
        if (spec.type == OptionType::Enum && spec.hasDefault)
            QVERIFY2(!spec.canonicalChoice(spec.defaultValue).isEmpty(), qPrintable(spec.name));
        if (!spec.requiredWhen.isEmpty())
            QVERIFY2(!spec.example.isEmpty(), qPrintable(spec.name + " is required but has no example"));
        if (spec.dimension() != Dimension::None && spec.hasDefault && !spec.defaultIsInfinite() &&
            spec.multiplicity == Multiplicity::Single)
            QVERIFY2(units::parse(spec.defaultValue, spec.dimension(), true).has_value(),
                     qPrintable(spec.name + " default " + spec.defaultValue));
    }
    for (const OptionSpec &spec : schema.options) {
        QVERIFY2(schema.conditions.contains(spec.usedWhen), qPrintable(spec.name + " uses " + spec.usedWhen));
        if (!spec.requiredWhen.isEmpty())
            QVERIFY2(schema.conditions.contains(spec.requiredWhen), qPrintable(spec.name));
    }
    for (auto it = schema.conditions.begin(); it != schema.conditions.end(); ++it) {
        QVERIFY(!it->description.isEmpty());
        for (const QString &name : condition::options(it->expression))
            QVERIFY2(names.contains(name), qPrintable(it.key() + " refers to " + name));
    }
    const QStringList kinds = {"required", "range", "greater", "greater_or_equal", "forbidden"};
    for (const OptionRule &rule : schema.rules) {
        QVERIFY2(kinds.contains(rule.kind), qPrintable(rule.kind));
        QVERIFY(!rule.message.isEmpty());
        QVERIFY2(schema.conditions.contains(rule.when), qPrintable(rule.message));
        QVERIFY2(schema.exitCodes.contains(rule.code), qPrintable(rule.message));
        for (const QString &name : rule.options + condition::options(rule.unless) + condition::options(rule.condition))
            QVERIFY2(names.contains(name), qPrintable(rule.message + " refers to " + name));
    }
    QCOMPARE(schema.exitCodeName(3), QStringLiteral("ERR_NOZSAFE"));
    QVERIFY(schema.grammar.has_value());
}

void TestCore::dumpFormat()
{
    const QByteArray json = R"({
      "format_version": 1, "program": "pcb2gcode", "version": "9.9.9", "git_commit": null,
      "future_key": {"anything": true},
      "types": {"length": {"units": [{"name": "mm", "aliases": ["millimeter"], "factor": 0.001},
                                     {"name": "m", "aliases": ["meter", "meters"], "factor": 1},
                                     {"name": "in", "aliases": ["inch"], "factor": 0.0254}]}},
      "conditions": {"always": {"description": "always", "expression": true},
                     "milling": {"description": "front or back is set",
                                 "expression": {"any": [{"set": "front"}, {"set": "back"}]}}},
      "groups": [{"id": "milling", "title": "Milling options"}],
      "options": [
        {"name": "front", "group": "milling", "description": "front", "type": "string", "default": null,
         "used_when": "always", "path": "input_file", "file_format": "gerber"},
        {"name": "mill-diameters", "group": "milling", "description": "d", "type": "length", "default": ["0"],
         "repeatable": true, "comma_separated": true, "multitoken": true, "used_when": "milling",
         "required_when": "milling", "example": "0.2mm"},
        {"name": "pre-milling-gcode", "group": "milling", "description": "g", "type": "string", "default": [],
         "repeatable": true, "used_when": "milling"},
        {"name": "mill-feed", "group": "milling", "description": "f", "type": "velocity", "default": null,
         "range": {"min": "0", "min_exclusive": true}, "used_when": "milling"},
        {"name": "tolerance", "group": "milling", "description": "t", "type": "double", "default": null,
         "bare_unit": "in", "metric_bare_unit": "mm", "used_when": "always"},
        {"name": "drill-front", "group": "milling", "description": "[DEPRECATED]", "type": "bool",
         "default": null, "implicit": "true", "deprecated": true, "replaced_by": "drill-side",
         "value_map": {"true": "front", "false": "back"}, "used_when": "always"},
        {"name": "help", "short": "?", "group": "milling", "description": "h", "type": "none", "default": null,
         "used_when": "always", "cli_only": true},
        {"name": "shiny", "group": "milling", "description": "a type from the future", "type": "hologram",
         "default": "x", "used_when": "always"}
      ],
      "rules": [{"kind": "greater", "options": ["mill-feed", "tolerance"], "when": "milling", "code": 7,
                 "message": "m"}, {"kind": "teleport", "options": ["front"], "when": "always", "code": 1,
                 "message": "unknown kind"}],
      "exit_codes": [{"code": 0, "name": "ERR_OK"}, {"code": 7, "name": "ERR_SEVEN"}]
    })";
    QString error;
    const OptionSchema schema = OptionSchema::fromJson(json, &error);
    QVERIFY2(schema.isValid(), qPrintable(error));
    QCOMPARE(schema.pcb2gcodeVersion, QStringLiteral("9.9.9"));
    QCOMPARE(schema.find("front")->path, PathKind::InputGerber);
    const OptionSpec *diameters = schema.find("mill-diameters");
    QCOMPARE(diameters->multiplicity, Multiplicity::CommaList);
    QCOMPARE(diameters->defaultValue, QStringLiteral("0"));
    QCOMPARE(diameters->requiredWhen, QStringLiteral("milling"));
    QCOMPARE(schema.find("pre-milling-gcode")->multiplicity, Multiplicity::Repeat);
    QVERIFY(schema.find("pre-milling-gcode")->hasDefault);
    QCOMPARE(schema.find("pre-milling-gcode")->defaultValue, QString());
    QCOMPARE(schema.find("mill-feed")->minimum, QStringLiteral("0"));
    QVERIFY(schema.find("mill-feed")->minimumExclusive);
    QVERIFY(schema.find("tolerance")->bareLength);
    QCOMPARE(schema.find("tolerance")->dimension(), Dimension::Length);
    QCOMPARE(schema.find("drill-front")->valueMap.value("true"), QStringLiteral("front"));
    QCOMPARE(schema.find("help")->shortName, QStringLiteral("?"));
    QCOMPARE(schema.find("shiny")->type, OptionType::String);
    QCOMPARE(schema.rules.size(), 2);
    QCOMPARE(schema.exitCodeName(7), QStringLiteral("ERR_SEVEN"));

    // The unit words come from the dump.
    QVERIFY(schema.grammar.has_value());
    units::setGrammar(*schema.grammar);
    QVERIFY(units::parse("2 meters", Dimension::Length, true).has_value());
    units::setGrammar(units::builtInGrammar());
    QVERIFY(!units::parse("2 meters", Dimension::Length, true).has_value());

    QByteArray future = json;
    future.replace("\"format_version\": 1", "\"format_version\": 2");
    QVERIFY(!OptionSchema::fromJson(future, &error).isValid());
    QVERIFY(!OptionSchema::fromJson("Error: Unknown option --dump-options", &error).isValid());
}

void TestCore::dumpExpressions()
{
    const QHash<QString, QString> values = {
        {"front", "board.gbr"}, {"tsp-2opt", "true"}, {"mill-feed-direction", "climb"},
        {"min-milldrill-hole-diameter", "inf"}, {"zchange-absolute", "false"}, {"drill-side", "auto"}};
    condition::Context ctx;
    ctx.isOption = [&](const QString &n) { return values.contains(n); };
    ctx.value = [&](const QString &n) -> std::optional<QString> {
        if (!values.contains(n))
            return std::nullopt;
        return values.value(n);
    };
    ctx.given = [](const QString &n) { return n == "front"; };
    ctx.kind = [](const QString &n) {
        return n == "tsp-2opt" || n == "zchange-absolute" ? condition::Kind::Bool : condition::Kind::Text;
    };
    auto eval = [&](const char *json, bool unknownValue = true, bool *sawUnknown = nullptr) {
        return condition::evaluate(QJsonDocument::fromJson(QByteArray("[") + json + "]").array().at(0), ctx,
                                   unknownValue, sawUnknown);
    };
    QVERIFY(eval("true"));
    QVERIFY(eval(R"({"set": "front"})"));
    QVERIFY(!eval(R"({"set": "back"})"));
    QVERIFY(eval(R"({"given": "front"})"));
    QVERIFY(!eval(R"({"given": "drill-side"})"));
    QVERIFY(!eval(R"({"finite": "min-milldrill-hole-diameter"})"));
    QVERIFY(eval(R"({"equal": ["tsp-2opt", "true"]})"));
    QVERIFY(eval(R"({"equal": ["zchange-absolute", "0"]})"));
    QVERIFY(eval(R"({"not": {"equal": ["mill-feed-direction", "any"]}})"));
    QVERIFY(eval(R"({"all": [{"set": "front"}, {"any": [{"set": "back"}, {"equal": ["tsp-2opt", "true"]}]}]})"));
    bool unknown = false;
    QVERIFY(eval(R"({"teleport": "front"})", true, &unknown));
    QVERIFY(unknown);
    unknown = false;
    QVERIFY(!eval(R"({"not": {"teleport": "front"}})", false, &unknown));
    QVERIFY(unknown);
    QCOMPARE(condition::options(QJsonDocument::fromJson(
                                    R"({"all": [{"set": "a"}, {"not": {"equal": ["b", "x"]}}]})").object()),
             (QStringList{"a", "b"}));
}

void TestCore::checkOutput()
{
    const auto result = pcb2gcode::parseCheckOutput(R"({
      "format_version": 1,
      "errors": [
        {"options": ["mill-feed"], "rule": "required:mill-feed",
         "message": "Missing --mill-feed, the milling feed rate. For example: --mill-feed=600mm/min", "code": 13},
        {"options": ["zsafe", "zwork"], "rule": "greater:zsafe,zwork",
         "message": "--zsafe must be higher than --zwork.", "code": 15, "future": 1},
        {"options": [], "rule": "something_new", "message": "A new kind of problem", "code": 100}
      ],
      "warnings": [{"options": ["zwork"], "message": "Warning: Engraving depth (--zwork) is greater than zero!"}]
    })");
    QVERIFY(result.has_value());
    QCOMPARE(result->errors.size(), 3);
    QCOMPARE(result->errors.at(1).options, (QStringList{"zsafe", "zwork"}));
    QCOMPARE(result->errors.at(1).rule, QStringLiteral("greater:zsafe,zwork"));
    QCOMPARE(result->errors.at(1).code, 15);
    QCOMPARE(result->errors.at(2).message, QStringLiteral("A new kind of problem"));
    QCOMPARE(result->warnings.size(), 1);
    QCOMPARE(result->warnings.first().options, QStringList{"zwork"});

    QVERIFY(!pcb2gcode::parseCheckOutput("").has_value());
    QVERIFY(!pcb2gcode::parseCheckOutput("Error: Unknown option --check-options").has_value());
    QVERIFY(!pcb2gcode::parseCheckOutput(R"({"format_version": 2, "errors": []})").has_value());
}

void TestCore::layoutMatchesSchema()
{
    const OptionSchema schema = OptionSchema::bundled();
    const Layout layout = Layout::bundled();
    QVERIFY(!layout.tabs.isEmpty());

    QSet<QString> placed;
    for (const QString &name : layout.placedOptions()) {
        QVERIFY2(schema.find(name), qPrintable("the layout places unknown option " + name));
        QVERIFY2(!placed.contains(name), qPrintable(name + " is placed twice"));
        placed.insert(name);
    }
    for (auto it = layout.hints.begin(); it != layout.hints.end(); ++it) {
        const OptionSpec *spec = schema.find(it.key());
        QVERIFY2(spec, qPrintable("the layout has a hint for unknown option " + it.key()));
        for (const QString &id : condition::identifiers(it->enabledIf))
            QVERIFY2(schema.find(id), qPrintable(it.key() + " is enabled by unknown option " + id));
        if (!it->preset.isEmpty() && spec->dimension() != Dimension::None &&
            spec->multiplicity == Multiplicity::Single)
            QVERIFY2(units::parse(it->preset, spec->dimension(), true).has_value(),
                     qPrintable(it.key() + " preset " + it->preset));
    }
    for (const OptionSpec &spec : schema.options)
        if (shownInForm(spec, layout))
            QVERIFY2(placed.contains(spec.name), qPrintable(spec.name + " isn't placed in gui-layout.json"));
}

void TestCore::parseHelp()
{
    const QString help = readFile(testData("pcb2gcode-3.2.0-help.txt"));
    QVERIFY(!help.isEmpty());
    const QList<HelpOption> options = OptionSchema::parseHelp(help);
    QCOMPARE(options.size(), 107);
    QHash<QString, HelpOption> byName;
    for (const HelpOption &o : options)
        byName.insert(o.name, o);

    QCOMPARE(byName.value("help").shortName, QStringLiteral("?"));
    QVERIFY(!byName.value("help").takesValue);
    QVERIFY(byName.value("voronoi").hasImplicit);
    QCOMPARE(byName.value("voronoi").defaultText, QStringLiteral("0"));
    QCOMPARE(byName.value("drill-side").defaultText, QStringLiteral("auto"));
    QCOMPARE(byName.value("optimise").defaultText, QStringLiteral("2.54e-06 m"));
    QCOMPARE(byName.value("min-milldrill-hole-diameter").defaultText, QStringLiteral("inf"));
    QCOMPARE(byName.value("al-probe-on").defaultText,
             QStringLiteral("(MSG, Attach the probe tool)@M0 ( Temporary machine stop. )"));
    QVERIFY(byName.value("drills-available").takesValue);
    QVERIFY(!byName.value("drills-available").hasDefault);
    QVERIFY(byName.value("milldrill").deprecated);
    QVERIFY(!byName.value("zsafe").deprecated);
    QCOMPARE(byName.value("zsafe").group, QStringLiteral("CNC options, common to all the milling, drilling, and cutting"));
    QVERIFY(byName.value("mcp").description.contains("Model Context Protocol"));
}

void TestCore::restrictToHelp()
{
    OptionSchema schema = OptionSchema::bundled();
    QList<HelpOption> help = OptionSchema::parseHelp(readFile(testData("pcb2gcode-3.2.0-help.txt")));
    help.erase(std::remove_if(help.begin(), help.end(), [](const HelpOption &h) { return h.name == "single-thread"; }),
               help.end());
    HelpOption added;
    added.name = "new-flag";
    added.takesValue = true;
    added.hasImplicit = true;
    added.hasDefault = true;
    added.defaultText = "0";
    added.description = "a new switch";
    help.append(added);

    schema.restrictTo(help);
    QVERIFY(schema.find("single-thread")->unsupported);
    QCOMPARE(schema.missingFromBinary, QStringList{"single-thread"});
    QCOMPARE(schema.addedFromHelp, QStringList{"new-flag"});
    const OptionSpec *flag = schema.find("new-flag");
    QVERIFY(flag);
    QCOMPARE(flag->type, OptionType::Bool);
    QCOMPARE(flag->defaultValue, QStringLiteral("false"));
    QCOMPARE(flag->group, QStringLiteral("other"));
}

void TestCore::parseConfig()
{
    const ConfigFile file = ConfigFile::parse(QStringLiteral(
        "# comment\n"
        "front = board-F.gbr\n"
        "mill-feed=360 in/min   # trailing comment\n"
        "#@#milldrill-diameter=3.0000\n"
        "[ignored section]\n"
        "pre-milling-gcode=M7\n"
        "pre-milling-gcode=M8\n"
        "al-setzzero=G92 Z0\n"
        "no equals sign\n"));
    QCOMPARE(file.entries.size(), 6);
    QCOMPARE(file.entries.at(0).key, QStringLiteral("front"));
    QCOMPARE(file.entries.at(0).value, QStringLiteral("board-F.gbr"));
    QCOMPARE(file.values("mill-feed"), QStringList{"360 in/min"});
    QCOMPARE(file.values("milldrill-diameter", true), QStringList{"3.0000"});
    QCOMPARE(file.values("pre-milling-gcode"), (QStringList{"M7", "M8"}));
    QCOMPARE(file.values("al-setzzero"), QStringList{"G92 Z0"});

    const ConfigFile again = ConfigFile::parse(file.serialize("header"));
    QCOMPARE(again.entries.size(), file.entries.size());
    for (int i = 0; i < file.entries.size(); i++) {
        QCOMPARE(again.entries.at(i).key, file.entries.at(i).key);
        QCOMPARE(again.entries.at(i).value, file.entries.at(i).value);
        QCOMPARE(again.entries.at(i).disabled, file.entries.at(i).disabled);
    }
}

void TestCore::migrateLegacyConfig()
{
    const OptionSchema schema = OptionSchema::bundled();
    ConfigFile file = ConfigFile::load(testData("legacy/old_gui.millproject"));
    QVERIFY(!file.entries.isEmpty());
    const QStringList notes = migrateLegacyEntries(file.entries, schema);
    QVERIFY(!notes.isEmpty());
    QCOMPARE(file.values("mill-diameters"), QStringList{"0.2"});
    QVERIFY(!file.contains("offset"));
    QCOMPARE(file.values("drill-side"), QStringList{"front"});
    QVERIFY(!file.contains("drill-front"));

    QList<ConfigEntry> entries = {{"milldrill", "true"}, {"cut-front", "false"}, {"g64", "0.01"},
                                  {"extra-passes", "2"}, {"mirror-absolute", "true"}};
    migrateLegacyEntries(entries, schema);
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries.at(0).key, QStringLiteral("min-milldrill-hole-diameter"));
    QCOMPARE(entries.at(0).value, QStringLiteral("0"));
    QCOMPARE(entries.at(1).key, QStringLiteral("cut-side"));
    QCOMPARE(entries.at(1).value, QStringLiteral("back"));
    QCOMPARE(entries.at(2).key, QStringLiteral("tolerance"));

    // n extra passes of a tool of diameter d overlapping by o isolate d + n × (d − o).
    QList<ConfigEntry> passes = {{"extra-passes", "2"}, {"mill-diameters", "0.2mm,0.5mm"}};
    migrateLegacyEntries(passes, schema);
    QCOMPARE(passes.size(), 2);
    QCOMPARE(passes.at(1).key, QStringLiteral("isolation-width"));
    QCOMPARE(passes.at(1).value, QStringLiteral("0.4mm"));
    QList<ConfigEntry> overlapped = {{"metric", "true"}, {"mill-diameters", "0.2"}, {"milling-overlap", "25%"},
                                     {"extra-passes", "2"}};
    migrateLegacyEntries(overlapped, schema);
    QCOMPARE(overlapped.last().value, QStringLiteral("0.5mm"));
    QList<ConfigEntry> noTool = {{"extra-passes", "3"}};
    QVERIFY(!migrateLegacyEntries(noTool, schema).isEmpty());
    QVERIFY(noTool.isEmpty());

    // With mill-diameters given, --offset is an extra offset and stays.
    QList<ConfigEntry> both = {{"offset", "0.05mm"}, {"mill-diameters", "0.2mm"}};
    migrateLegacyEntries(both, schema);
    QCOMPARE(both.size(), 2);
}

void TestCore::previewsByFileName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (const char *name : {"processed_front_final_0.svg", "processed_front_final_1.svg", "processed_front_0.svg",
                             "traced_front_final_0.svg", "outp0_original_front.svg", "outp1_masked_front.svg",
                             "original_drill.svg", "contentions_front_1.svg", "processed_outline.svg",
                             "unrelated.svg", "front.ngc"}) {
        QFile f(QDir(dir.path()).filePath(name));
        QVERIFY(f.open(QIODevice::WriteOnly));
    }
    const QList<PreviewImage> images = previewsFromFileNames(dir.path());
    QStringList labels;
    for (const PreviewImage &i : images)
        labels << i.label();
    QCOMPARE(labels, (QStringList{
                         "Front: toolpaths and copper, tool 1",
                         "Front: toolpaths and copper, tool 2",
                         "Outline: cutting path and board",
                         "Front: toolpaths, tool 1",
                         "Front: clearance problems, tool 2",
                         "Front: toolpaths and copper, tool 1 (before optimisation)",
                         "Front: copper inside the outline",
                         "Front: input",
                         "Drill: holes",
                     }));
}

void TestCore::previewsByManifest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile f(QDir(dir.path()).filePath("manifest.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(R"({
      "format_version": 1,
      "files": [
        {"path": "back.ngc", "type": "gcode", "layer": "back", "tools": [{"number": 1, "diameter_mm": 0.2}]},
        {"path": "outp0_original_back.svg", "type": "svg", "layer": "back", "stage": "original"},
        {"path": "processed_back_final_1.svg", "type": "svg", "layer": "back", "stage": "processed",
         "final": true, "tool_index": 1, "tool_diameter_mm": 0.8},
        {"path": "processed_back_final_0.svg", "type": "svg", "layer": "back", "stage": "processed",
         "final": true, "tool_index": 0, "tool_diameter_mm": 0.2},
        {"path": "sub/processed_back_0.svg", "type": "svg", "layer": "back", "stage": "processed",
         "final": false, "tool_index": 0, "tool_diameter_mm": 0.2},
        {"path": "original_milldrill.svg", "type": "svg", "layer": "milldrill", "stage": "original", "new_key": 1}
      ]
    })");
    f.close();
    const QList<PreviewImage> images = previewsFromManifest(f.fileName());
    QCOMPARE(images.size(), 5);
    QCOMPARE(images.at(0).label(), QStringLiteral("Back: toolpaths and copper, tool 1 (0.2 mm)"));
    QCOMPARE(images.at(1).label(), QStringLiteral("Back: toolpaths and copper, tool 2 (0.8 mm)"));
    QCOMPARE(images.at(2).label(), QStringLiteral("Back: toolpaths and copper, tool 1 (0.2 mm) (before optimisation)"));
    QCOMPARE(images.at(2).path, QDir(dir.path()).filePath("sub/processed_back_0.svg"));
    QCOMPARE(images.at(3).label(), QStringLiteral("Back: input"));
    QCOMPARE(images.at(4).label(), QStringLiteral("Milldrill: holes"));
}

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
