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

#include "configfile.h"
#include "layout.h"
#include "optionform.h"
#include "optionschema.h"
#include "previewpane.h"

#include <QGraphicsView>
#include <QTabWidget>
#include <QWheelEvent>
#include <QtTest>

namespace {

QString testData(const QString &name)
{
    return QDir(qEnvironmentVariable("PCB2GCODEGUI_TEST_DATA")).filePath(name);
}

bool hasArgWithPrefix(const QStringList &args, const QString &prefix)
{
    for (const QString &a : args)
        if (a.startsWith(prefix))
            return true;
    return false;
}

}  // namespace

class TestForm : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void defaults();
    void millingNeedsCopperFiles();
    void unitSystem();
    void listsGetUnits();
    void optionalValues();
    void rules();
    void constraints();
    void configRoundTrip();
    void legacyConfig();
    void unknownOptionsAreShown();
    void unsupportedOptionsAreHidden();
    void previewZoomSticks();

private:
    OptionForm *form = nullptr;
};

void TestForm::init()
{
    form = new OptionForm;
    form->build(OptionSchema::bundled(), Layout::bundled());
    form->resetAll(true);
}

void TestForm::cleanup()
{
    delete form;
    form = nullptr;
}

void TestForm::defaults()
{
    const QStringList args = form->arguments();
    QVERIFY(args.contains("--metric=true"));
    QVERIFY(args.contains("--metricoutput=true"));
    QVERIFY(args.contains("--zsafe=5mm"));
    QVERIFY(args.contains("--zchange=10mm"));
    QVERIFY(args.contains("--zero-start=true"));
    // pcb2gcode's own defaults aren't repeated, and GUI-managed options never appear.
    QVERIFY(!hasArgWithPrefix(args, "--tile-x"));
    QVERIFY(!hasArgWithPrefix(args, "--optimise"));
    QVERIFY(!hasArgWithPrefix(args, "--vectorial"));
    QVERIFY(!hasArgWithPrefix(args, "--noconfigfile"));
    QVERIFY(!hasArgWithPrefix(args, "--spindown-time"));
    QVERIFY(form->problems().isEmpty());
}

void TestForm::millingNeedsCopperFiles()
{
    QVERIFY(!hasArgWithPrefix(form->arguments(), "--zwork"));
    QVERIFY(form->setOptionValues("back", {testData("example_board/example_board-B.Cu.gbr")}));
    const QStringList args = form->arguments();
    QVERIFY(args.contains("--zwork=-0.05mm"));
    QVERIFY(args.contains("--mill-feed=600mm/min"));
    QVERIFY(args.contains("--mill-speed=10000rpm"));
    QVERIFY(args.contains("--mill-diameters=0.2mm"));
    QVERIFY(!hasArgWithPrefix(args, "--mill-vertfeed"));
    QVERIFY(!hasArgWithPrefix(args, "--zdrill"));
    QVERIFY(!hasArgWithPrefix(args, "--cutter-diameter"));
}

void TestForm::unitSystem()
{
    QVERIFY(form->setOptionValues("zsafe", {"5mm"}));
    QVERIFY(form->setOptionValues("metric", {"false"}));
    QVERIFY(!form->metric());
    QStringList args = form->arguments();
    QVERIFY2(args.contains("--zsafe=0.19685in"), qPrintable(args.join(' ')));
    QVERIFY(!hasArgWithPrefix(args, "--metric="));
    QVERIFY(form->setOptionValues("metric", {"true"}));
    args = form->arguments();
    QVERIFY2(args.contains("--zsafe=5mm"), qPrintable(args.join(' ')));
    QVERIFY(args.contains("--metric=true"));
}

void TestForm::listsGetUnits()
{
    QVERIFY(form->setOptionValues("front", {testData("example_board/example_board-F.Cu.gbr")}));
    QVERIFY(form->setOptionValues("mill-diameters", {"0.2, 0.5mm, 10mil"}));
    QVERIFY(form->arguments().contains("--mill-diameters=0.2mm,0.5mm,10mil"));
    // Switching units doesn't change what was typed.
    QVERIFY(form->setOptionValues("metric", {"false"}));
    QVERIFY(form->arguments().contains("--mill-diameters=0.2mm,0.5mm,10mil"));

    QVERIFY(form->setOptionValues("drill", {testData("example_board/example_board-PTH.drl")}));
    QVERIFY(form->setOptionValues("drills-available", {"0.031, 0.04:-0.002:+0.004"}));
    QVERIFY(form->arguments().contains("--drills-available=0.031in,0.04in:-0.002in:+0.004in"));
}

void TestForm::optionalValues()
{
    QVERIFY(form->setOptionValues("front", {testData("example_board/example_board-F.Cu.gbr")}));
    OptionEditor *infeed = form->editor("mill-infeed");
    QVERIFY(infeed && infeed->hasOverride());
    QVERIFY(!infeed->isOverridden());
    QVERIFY(!hasArgWithPrefix(form->arguments(), "--mill-infeed"));
    infeed->setOverridden(true);
    QVERIFY(form->arguments().contains("--mill-infeed=0.05mm"));
    QVERIFY(form->setOptionValues("mill-infeed", {"0.1mm"}));
    QVERIFY(form->arguments().contains("--mill-infeed=0.1mm"));
    infeed->setOverridden(false);
    QVERIFY(!hasArgWithPrefix(form->arguments(), "--mill-infeed"));

    // An infinite default can only be had by leaving the option out.
    OptionEditor *backtrack = form->editor("backtrack");
    QVERIFY(backtrack && backtrack->hasOverride());
    QVERIFY(!hasArgWithPrefix(form->arguments(), "--backtrack"));
    backtrack->setOverridden(true);
    QVERIFY2(form->arguments().contains("--backtrack=127mm/s"), qPrintable(form->arguments().join(' ')));
}

void TestForm::rules()
{
    QVERIFY(form->editor("isolation-width")->ruleEnabled());
    QVERIFY(!form->editor("preserve-thermal-reliefs")->ruleEnabled());
    QVERIFY(form->setOptionValues("voronoi", {"true"}));
    QVERIFY(!form->editor("isolation-width")->ruleEnabled());
    QVERIFY(form->editor("preserve-thermal-reliefs")->ruleEnabled());

    QVERIFY(!form->editor("mill-feed-direction")->ruleEnabled());
    QVERIFY(form->setOptionValues("tsp-2opt", {"false"}));
    QVERIFY(form->editor("mill-feed-direction")->ruleEnabled());

    QVERIFY(!form->editor("software")->ruleEnabled());
    QVERIFY(form->setOptionValues("al-back", {"true"}));
    QVERIFY(form->editor("software")->ruleEnabled());
    QVERIFY(!form->editor("al-probecode")->ruleEnabled());
    QVERIFY(form->setOptionValues("software", {"Custom"}));
    QVERIFY(form->editor("al-probecode")->ruleEnabled());

    QVERIFY(!form->editor("milldrill-stepover")->ruleEnabled());
    form->editor("min-milldrill-hole-diameter")->setOverridden(true);
    QVERIFY(form->editor("milldrill-stepover")->ruleEnabled());

    QVERIFY(!form->editor("bridgesnum")->ruleEnabled());
    QVERIFY(form->setOptionValues("bridges", {"2mm"}));
    QVERIFY(form->editor("bridgesnum")->ruleEnabled());

    // A disabled option is not passed even if it differs from the default.
    QVERIFY(form->setOptionValues("front", {testData("example_board/example_board-F.Cu.gbr")}));
    QVERIFY(form->setOptionValues("mill-feed-direction", {"climb"}));
    QVERIFY(form->arguments().contains("--mill-feed-direction=climb"));
    QVERIFY(form->setOptionValues("tsp-2opt", {"true"}));
    QVERIFY(!hasArgWithPrefix(form->arguments(), "--mill-feed-direction"));
}

void TestForm::constraints()
{
    QVERIFY(form->setOptionValues("front", {testData("example_board/example_board-F.Cu.gbr")}));
    QVERIFY(form->problems().isEmpty());
    QVERIFY(form->setOptionValues("zsafe", {"-1mm"}));
    QVERIFY2(form->problems().contains("--zsafe must be higher than --zwork."), qPrintable(form->problems().join('\n')));
    QVERIFY(form->setOptionValues("zsafe", {"3mm"}));
    QVERIFY(form->setOptionValues("mill-feed", {"0"}));
    QVERIFY(!form->problems().isEmpty());
}

void TestForm::configRoundTrip()
{
    const ConfigFile config = ConfigFile::load(testData("example_board/two_sided.millproject"));
    QVERIFY(!config.entries.isEmpty());
    const QStringList notes = form->applyConfig(config, testData("example_board"));
    QVERIFY2(notes.isEmpty(), qPrintable(notes.join('\n')));
    const QStringList args = form->arguments();
    QVERIFY(args.contains("--front=" + QDir::cleanPath(testData("example_board/example_board-F.Cu.gbr"))));
    QVERIFY(args.contains("--pre-milling-gcode=(coolant on)"));

    const ConfigFile saved = ConfigFile::parse(form->toConfig().serialize("test"));
    OptionForm other;
    other.build(OptionSchema::bundled(), Layout::bundled());
    other.resetAll(true);
    other.applyConfig(saved);
    QCOMPARE(other.arguments(), args);
}

void TestForm::legacyConfig()
{
    const ConfigFile config = ConfigFile::load(testData("legacy/old_gui.millproject"));
    const QStringList notes = form->applyConfig(config, testData("legacy"));
    QVERIFY(!notes.isEmpty());
    const QStringList args = form->arguments();
    QVERIFY(args.contains("--mill-diameters=0.2mm"));
    QVERIFY(args.contains("--drill-side=front"));
    QVERIFY(args.contains("--tolerance=0.01"));
    QVERIFY(args.contains("--cut-vertfeed=60mm/min"));
    QVERIFY(!hasArgWithPrefix(args, "--offset"));
    QVERIFY(!hasArgWithPrefix(args, "--milldrill-diameter"));
    // Inactive (#@#) values are remembered, not passed.
    QVERIFY(form->editor("milldrill-diameter")->hasOverride());
    QVERIFY(!form->editor("milldrill-diameter")->isOverridden());
}

void TestForm::unknownOptionsAreShown()
{
    OptionSchema schema = OptionSchema::bundled();
    OptionSpec extra;
    extra.name = "future-option";
    extra.group = "milling";
    extra.type = OptionType::Length;
    extra.hasDefault = true;
    extra.defaultValue = "0";
    extra.description = "added to pcb2gcode after this GUI";
    schema.options.append(extra);
    form->build(schema, Layout::bundled());
    form->resetAll(true);
    OptionEditor *e = form->editor("future-option");
    QVERIFY(e);
    QVERIFY(!hasArgWithPrefix(form->arguments(), "--future-option"));
    QVERIFY(form->setOptionValues("future-option", {"0.3mm"}));
    QVERIFY(form->arguments().contains("--future-option=0.3mm"));
}

void TestForm::unsupportedOptionsAreHidden()
{
    OptionSchema schema = OptionSchema::bundled();
    schema.find("milldrill-stepover")->unsupported = true;
    form->build(schema, Layout::bundled());
    QVERIFY(!form->editor("milldrill-stepover"));
    QVERIFY(!form->supports("milldrill-stepover"));
    QVERIFY(!form->supports("manifest"));
}

void TestForm::previewZoomSticks()
{
    PreviewPane pane;
    pane.resize(600, 400);
    pane.show();
    QVERIFY(QTest::qWaitForWindowExposed(&pane));
    PreviewImage image;
    image.path = ":/icons/pcb2gcodeGUI.svg";
    image.layer = "front";
    image.stage = "original";
    pane.setImages({image}, QString());
    QTest::qWait(20);
    const double fitted = pane.view()->transform().m11();
    QVERIFY(fitted > 0);

    // Zooming adds scroll bars, which resizes the viewport; that must not fit the board again.
    QWidget *viewport = pane.view()->viewport();
    const QPointF pos(viewport->width() / 2.0, viewport->height() / 2.0);
    for (int i = 0; i < 5; i++) {
        QWheelEvent wheel(pos, viewport->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(viewport, &wheel);
    }
    QTest::qWait(50);
    QVERIFY2(pane.view()->transform().m11() > fitted * 2,
             qPrintable(QString::number(pane.view()->transform().m11() / fitted)));
}

QTEST_MAIN(TestForm)
#include "tst_form.moc"
