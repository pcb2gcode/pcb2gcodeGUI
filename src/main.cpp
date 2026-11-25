/*
 * Copyright (c) 2015-2016 Nicola Corna (nicola@corna.info)
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
#include "mainwindow.h"
#include "optionform.h"
#include "pcb2gcode.h"
#include "previews.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QSvgRenderer>
#include <QTextStream>
#include <cstring>

namespace {

int printCommandLine(const QString &configPath, const QString &pcb2gcodePath, bool bundled)
{
    QTextStream out(stdout), err(stderr);
    const pcb2gcode::Probe probe = pcb2gcode::probe(pcb2gcode::locate(pcb2gcodePath), bundled);
    err << probe.schemaNote << '\n';
    QString error;
    const ConfigFile config = ConfigFile::load(configPath, &error);
    if (!error.isEmpty()) {
        err << "Can't read " << configPath << ": " << error << '\n';
        return 1;
    }
    OptionForm form;
    form.build(probe.schema, Layout::bundled());
    for (const QString &note : form.applyConfig(config, QFileInfo(configPath).absolutePath()))
        err << "note: " << note << '\n';
    std::optional<pcb2gcode::CheckResult> check;
    if (!probe.program.isEmpty() && form.supports(QStringLiteral("check-options")))
        check = pcb2gcode::checkOptions(probe.program, QStringList{QStringLiteral("--noconfigfile")} + form.arguments());
    if (check) {
        form.setReportedProblems(*check);
        err << "Options checked by pcb2gcode " << pcb2gcode::kCheckOptionsFlag << '\n';
    } else {
        err << "Options checked by the GUI's copy of pcb2gcode's rules\n";
    }
    for (const QString &problem : form.problems())
        err << "problem: " << problem << '\n';
    for (const QString &warning : form.warnings())
        err << "warning: " << warning << '\n';
    out << "--noconfigfile\n";
    for (const QString &arg : form.arguments())
        out << arg << '\n';
    return 0;
}

int checkPreviews(const QString &directory, const QString &manifest)
{
    QTextStream out(stdout);
    bool fromManifest = false;
    const QList<PreviewImage> images = collectPreviews(directory, manifest, &fromManifest);
    int bad = 0;
    for (const PreviewImage &image : images) {
        const bool ok = QSvgRenderer(image.path).isValid();
        bad += ok ? 0 : 1;
        out << (ok ? "ok    " : "BROKEN") << "  " << image.label() << "  " << QFileInfo(image.path).fileName()
            << '\n';
    }
    out << images.size() << " preview images, " << bad << " broken, found "
        << (fromManifest ? "in the manifest" : "by file name") << '\n';
    return images.isEmpty() || bad > 0 ? 1 : 0;
}

}  // namespace

int main(int argc, char *argv[])
{
    // The command-line modes work without a display.
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--print-command-line") || !std::strcmp(argv[i], "--check-previews")) {
            if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
                qputenv("QT_QPA_PLATFORM", "offscreen");
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("pcb2gcodeGUI"));
    QApplication::setApplicationVersion(QStringLiteral(PCB2GCODEGUI_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("pcb2gcodeGUI"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/pcb2gcodeGUI.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A graphical front end for pcb2gcode."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption pcb2gcodeOption(QStringLiteral("pcb2gcode"),
                                             QStringLiteral("Use this pcb2gcode executable."),
                                             QStringLiteral("path"));
    const QCommandLineOption bundledOption(QStringLiteral("bundled-options"),
                                           QStringLiteral("Use the built-in option list even if pcb2gcode can "
                                                          "describe its own options."));
    const QCommandLineOption printOption(QStringLiteral("print-command-line"),
                                         QStringLiteral("Print the pcb2gcode arguments for a configuration file, "
                                                        "one per line, and exit."),
                                         QStringLiteral("millproject"));
    const QCommandLineOption previewsOption(QStringLiteral("check-previews"),
                                            QStringLiteral("List the preview images in a pcb2gcode output folder, "
                                                           "check that they load, and exit."),
                                            QStringLiteral("directory"));
    const QCommandLineOption manifestOption(QStringLiteral("manifest"),
                                            QStringLiteral("With --check-previews, the --manifest file to read."),
                                            QStringLiteral("file"));
    parser.addOptions({pcb2gcodeOption, bundledOption, printOption, previewsOption, manifestOption});
    parser.addPositionalArgument(QStringLiteral("millproject"), QStringLiteral("Configuration file to open."),
                                 QStringLiteral("[millproject]"));
    parser.process(app);

    if (parser.isSet(printOption))
        return printCommandLine(parser.value(printOption), parser.value(pcb2gcodeOption),
                                parser.isSet(bundledOption));
    if (parser.isSet(previewsOption))
        return checkPreviews(parser.value(previewsOption), parser.value(manifestOption));

    MainWindow window(parser.value(pcb2gcodeOption), parser.isSet(bundledOption));
    window.show();
    if (!parser.positionalArguments().isEmpty())
        window.openConfig(parser.positionalArguments().constFirst());
    return app.exec();
}
