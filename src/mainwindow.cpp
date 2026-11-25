/*
 * Copyright (c) 2015-2016 Nicola Corna (nicola@corna.info)
 * Copyright (c) 2025 Enrique Condes (kosmerebel@gmail.com)
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

#include "mainwindow.h"

#include "configfile.h"
#include "layout.h"
#include "optionform.h"
#include "outputwindow.h"
#include "previewpane.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace {

const char *const kWikiUrl = "https://github.com/pcb2gcode/pcb2gcode/wiki";
const char *const kDefaultConfigName = "default_millproject";
const char *const kPreviewManifest = "manifest.json";

QStringList withoutOptions(QStringList args, const QStringList &names)
{
    args.erase(std::remove_if(args.begin(), args.end(),
                              [&names](const QString &a) {
                                  for (const QString &n : names)
                                      if (a.startsWith("--" + n + "="))
                                          return true;
                                  return false;
                              }),
               args.end());
    return args;
}

void clearDirectory(const QString &path)
{
    QDir dir(path);
    for (const QString &f : dir.entryList(QDir::Files))
        dir.remove(f);
}

}  // namespace

MainWindow::MainWindow(const QString &pcb2gcodePath, bool bundledOptions, QWidget *parent)
    : QMainWindow(parent), m_pcb2gcodePath(pcb2gcodePath), m_bundledOptions(bundledOptions)
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, appData);
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("pcb2gcodeGUI"),
                               QString(), this);

    createWidgets();
    createMenus();

    m_previewProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_previewProcess, &QProcess::finished, this, &MainWindow::previewFinished);
    connect(&m_previewProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_preview->setBusy(false);
            m_preview->setMessage(tr("pcb2gcode failed to start: %1").arg(m_previewProcess.errorString()), true);
        }
    });
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(700);
    connect(&m_previewTimer, &QTimer::timeout, this, &MainWindow::startPreview);

    loadSettings();
    if (m_pcb2gcodePath.isEmpty())
        m_pcb2gcodePath = m_settings->value(QStringLiteral("pcb2gcodePath")).toString();
    setPcb2gcode(m_pcb2gcodePath);
    loadStartupConfig();
}

void MainWindow::createWidgets()
{
    m_banner = new QFrame;
    m_banner->setObjectName(QStringLiteral("banner"));
    m_banner->setStyleSheet(QStringLiteral(
        "#banner { background: #fff4d6; border: 1px solid #e3c46b; border-radius: 4px; }"));
    m_bannerText = new QLabel;
    m_bannerText->setWordWrap(true);
    auto *locate = new QPushButton(tr("Locate pcb2gcode…"));
    connect(locate, &QPushButton::clicked, this, &MainWindow::locatePcb2gcode);
    auto *bannerLayout = new QHBoxLayout(m_banner);
    bannerLayout->addWidget(m_bannerText, 1);
    bannerLayout->addWidget(locate);
    m_banner->setVisible(false);

    m_form = new OptionForm;
    connect(m_form, &OptionForm::changed, this, &MainWindow::formChanged);
    connect(m_form, &OptionForm::fileChosen, this, [this] {
        if (m_autoPreview->isChecked())
            startPreview();
    });

    m_problems = new QLabel;
    m_problems->setWordWrap(true);
    m_problems->setStyleSheet(QStringLiteral("color: #9a5b00;"));
    m_problems->setVisible(false);

    auto *commandButton = new QPushButton(tr("Command line"));
    connect(commandButton, &QPushButton::clicked, this, &MainWindow::showCommandLine);
    m_previewButton = new QPushButton(tr("Preview"));
    connect(m_previewButton, &QPushButton::clicked, this, &MainWindow::startPreview);
    m_generateButton = new QPushButton(tr("Generate G-code"));
    m_generateButton->setDefault(true);
    connect(m_generateButton, &QPushButton::clicked, this, &MainWindow::generate);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(commandButton);
    buttons->addStretch(1);
    buttons->addWidget(m_previewButton);
    buttons->addWidget(m_generateButton);

    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_form, 1);
    leftLayout->addWidget(m_problems);
    leftLayout->addLayout(buttons);

    m_preview = new PreviewPane;
    connect(m_preview, &PreviewPane::refreshRequested, this, &MainWindow::startPreview);

    m_splitter = new QSplitter;
    m_splitter->addWidget(left);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({640, 660});

    auto *central = new QWidget;
    auto *v = new QVBoxLayout(central);
    v->addWidget(m_banner);
    v->addWidget(m_splitter, 1);
    setCentralWidget(central);

    m_status = new QLabel;
    statusBar()->addPermanentWidget(m_status, 1);
    resize(1300, 860);
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New configuration"), QKeySequence::New, this, &MainWindow::newConfig);
    file->addAction(tr("&Open configuration…"), QKeySequence::Open, this, &MainWindow::openConfigDialog);
    file->addAction(tr("&Save configuration"), QKeySequence::Save, this, &MainWindow::saveConfig);
    file->addAction(tr("Save configuration &as…"), QKeySequence::SaveAs, this, &MainWindow::saveConfigAs);
    file->addSeparator();
    file->addAction(tr("Save as &default configuration"), this, &MainWindow::saveDefaultConfig);
    file->addAction(tr("&Load default configuration"), this, &MainWindow::loadDefaultConfig);
    file->addAction(tr("&Reset default configuration…"), this, &MainWindow::resetDefaultConfig);
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *run = menuBar()->addMenu(tr("&Run"));
    run->addAction(tr("&Preview"), QKeySequence::Refresh, this, &MainWindow::startPreview);
    run->addAction(tr("&Generate G-code"), QKeySequence(Qt::CTRL | Qt::Key_G), this, &MainWindow::generate);
    run->addAction(tr("Show &command line"), QKeySequence(Qt::CTRL | Qt::Key_L), this,
                   &MainWindow::showCommandLine);
    run->addSeparator();
    m_autoPreview = run->addAction(tr("&Refresh preview automatically"));
    m_autoPreview->setCheckable(true);
    m_autoPreview->setChecked(true);

    QMenu *settings = menuBar()->addMenu(tr("&Settings"));
    settings->addAction(tr("pcb2gcode &executable…"), this, &MainWindow::locatePcb2gcode);
    settings->addAction(tr("&Reload pcb2gcode options"), this, [this] {
        const ConfigFile current = m_form->toConfig();
        setPcb2gcode(m_pcb2gcodePath);
        showNotes(tr("Reloading"), m_form->applyConfig(current));
    });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("pcb2gcode &wiki"), this,
                    [] { QDesktopServices::openUrl(QUrl(QString::fromLatin1(kWikiUrl))); });
    help->addAction(tr("About &pcb2gcode"), this, &MainWindow::aboutPcb2gcode);
    help->addAction(tr("&About pcb2gcodeGUI"), this, &MainWindow::aboutGui);
    help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::loadSettings()
{
    m_autoPreview->setChecked(m_settings->value(QStringLiteral("autoPreview"), true).toBool());
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    m_form->rememberDirectory(PathKind::InputGerber, m_settings->value(QStringLiteral("lastGcodeDir"), home).toString());
    m_form->rememberDirectory(PathKind::InputGcode, m_settings->value(QStringLiteral("lastPreambleDir"), home).toString());
    m_form->rememberDirectory(PathKind::InputText, m_settings->value(QStringLiteral("lastPreambleDir"), home).toString());
    m_form->rememberDirectory(PathKind::Directory, m_settings->value(QStringLiteral("lastOutputDir"), home).toString());
    if (m_settings->contains(QStringLiteral("Window/geometry")))
        restoreGeometry(m_settings->value(QStringLiteral("Window/geometry")).toByteArray());
    else if (m_settings->contains(QStringLiteral("Window/width")))
        resize(m_settings->value(QStringLiteral("Window/width")).toInt(),
               m_settings->value(QStringLiteral("Window/height")).toInt());
    if (m_settings->contains(QStringLiteral("Window/splitter")))
        m_splitter->restoreState(m_settings->value(QStringLiteral("Window/splitter")).toByteArray());
}

void MainWindow::saveSettings()
{
    m_settings->setValue(QStringLiteral("autoPreview"), m_autoPreview->isChecked());
    m_settings->setValue(QStringLiteral("lastGcodeDir"), m_form->browseDirectory(PathKind::InputGerber));
    m_settings->setValue(QStringLiteral("lastPreambleDir"), m_form->browseDirectory(PathKind::InputGcode));
    m_settings->setValue(QStringLiteral("lastOutputDir"), m_form->browseDirectory(PathKind::Directory));
    m_settings->setValue(QStringLiteral("Window/geometry"), saveGeometry());
    m_settings->setValue(QStringLiteral("Window/splitter"), m_splitter->saveState());
    m_settings->setValue(QStringLiteral("pcb2gcodePath"), m_pcb2gcodePath);
}

void MainWindow::setPcb2gcode(const QString &path)
{
    m_previewProcess.kill();
    m_previewProcess.waitForFinished(1000);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_probe = pcb2gcode::probe(pcb2gcode::locate(path), m_bundledOptions);
    QApplication::restoreOverrideCursor();
    m_form->build(m_probe.schema, Layout::bundled());

    const bool found = !m_probe.program.isEmpty();
    m_banner->setVisible(!found);
    if (!found)
        m_bannerText->setText(tr("<b>pcb2gcode wasn't found.</b> You can still edit and save configurations; "
                                 "to preview and generate G-code, install pcb2gcode or tell the GUI where it is."));
    m_previewButton->setEnabled(found);
    m_generateButton->setEnabled(found);
    updateStatus();
}

void MainWindow::loadStartupConfig()
{
    if (QFileInfo::exists(defaultConfigPath())) {
        QString error;
        const ConfigFile file = ConfigFile::load(defaultConfigPath(), &error);
        const QStringList notes = m_form->applyConfig(file);
        if (!notes.isEmpty())
            statusBar()->showMessage(tr("Default configuration: %1").arg(notes.join("; ")), 15000);
    } else {
        m_form->resetAll(true);
    }
    m_configPath.clear();
    updateTitle();
}

void MainWindow::updateTitle()
{
    setWindowTitle(m_configPath.isEmpty() ? QStringLiteral("pcb2gcodeGUI")
                                          : QStringLiteral("%1 — pcb2gcodeGUI").arg(QFileInfo(m_configPath).fileName()));
}

void MainWindow::updateStatus()
{
    QString text;
    if (!m_probe.program.isEmpty())
        text = tr("pcb2gcode %1 (%2). ").arg(m_probe.version, QDir::toNativeSeparators(m_probe.program));
    text += m_probe.schemaNote;
    if (m_probe.olderThanSchema)
        text += tr(". This pcb2gcode is older than the built-in list; some defaults may differ.");
    if (!m_probe.schema.addedFromHelp.isEmpty())
        text += tr(" New options found: %1.").arg(m_probe.schema.addedFromHelp.join(", "));
    m_status->setText(text);
}

bool MainWindow::hasInputFiles() const
{
    for (const QString &name : QStringList{"front", "back", "outline", "drill"}) {
        OptionEditor *e = m_form->editor(name);
        if (e && !e->values().isEmpty())
            return true;
    }
    return false;
}

void MainWindow::formChanged()
{
    const QStringList problems = m_form->problems();
    m_problems->setVisible(!problems.isEmpty());
    if (!problems.isEmpty()) {
        QStringList shown = problems.mid(0, 4);
        if (problems.size() > 4)
            shown << tr("…and %n more", nullptr, int(problems.size() - 4));
        m_problems->setText(QStringLiteral("• ") + shown.join(QStringLiteral("\n• ")));
    }
    schedulePreview();
}

QStringList MainWindow::commandArguments() const
{
    return QStringList{QStringLiteral("--noconfigfile")} + m_form->arguments();
}

void MainWindow::schedulePreview()
{
    if (m_autoPreview->isChecked() && !m_probe.program.isEmpty() && hasInputFiles())
        m_previewTimer.start();
}

void MainWindow::startPreview()
{
    m_previewTimer.stop();
    if (m_probe.program.isEmpty())
        return;
    if (!hasInputFiles()) {
        m_preview->setMessage(tr("Choose a front, back, outline or drill file to see a preview."));
        return;
    }
    if (!m_tempDir.isValid()) {
        m_preview->setMessage(tr("Can't create a temporary folder for previews."), true);
        return;
    }
    if (m_previewProcess.state() != QProcess::NotRunning) {
        m_previewAgain = true;
        m_previewProcess.kill();
        return;
    }
    const QString dir = QDir(m_tempDir.path()).filePath(QStringLiteral("preview"));
    QDir().mkpath(dir);
    clearDirectory(dir);
    // A full run: toolpath images are only made when exporting G-code.
    QStringList args = withoutOptions(commandArguments(), {"output-dir", "manifest", "no-export"});
    args << QStringLiteral("--output-dir=") + dir;
    if (m_form->supports(QStringLiteral("manifest")))
        args << QStringLiteral("--manifest=") + kPreviewManifest;
    m_preview->setBusy(true);
    m_previewProcess.setWorkingDirectory(dir);
    m_previewProcess.start(m_probe.program, args, QIODevice::ReadOnly);
}

void MainWindow::previewFinished(int exitCode, QProcess::ExitStatus status)
{
    const QString output = QString::fromLocal8Bit(m_previewProcess.readAll());
    m_preview->setBusy(false);
    if (m_previewAgain) {
        m_previewAgain = false;
        startPreview();
        return;
    }
    const QString dir = QDir(m_tempDir.path()).filePath(QStringLiteral("preview"));
    if (status == QProcess::NormalExit && exitCode == 0) {
        const QList<PreviewImage> images = collectPreviews(dir, QDir(dir).filePath(kPreviewManifest));
        m_preview->setImages(images, QString());
        return;
    }
    QStringList errors;
    for (const QString &line : output.split('\n'))
        if (line.startsWith(QStringLiteral("Error")))
            errors << line.trimmed();
    m_preview->setMessage(errors.isEmpty() ? tr("pcb2gcode couldn't make a preview (exit code %1).").arg(exitCode)
                                           : errors.join('\n'),
                          true, output);
}

void MainWindow::generate()
{
    if (m_probe.program.isEmpty())
        return;
    OptionEditor *outputDir = m_form->editor(QStringLiteral("output-dir"));
    if (outputDir && outputDir->values().isEmpty()) {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose the output folder"),
                                                              m_form->browseDirectory(PathKind::Directory));
        if (dir.isEmpty())
            return;
        m_form->rememberDirectory(PathKind::Directory, dir);
        m_form->setOptionValues(QStringLiteral("output-dir"), {dir});
    }
    m_previewTimer.stop();
    if (m_previewProcess.state() != QProcess::NotRunning) {
        m_previewProcess.kill();
        m_previewProcess.waitForFinished(2000);
    }

    m_runOutputDir = outputDir ? outputDir->values().value(0) : QDir::currentPath();
    QStringList args = commandArguments();
    if (m_form->supports(QStringLiteral("manifest")) && m_tempDir.isValid()) {
        const QString manifest = QDir(m_tempDir.path()).filePath(QStringLiteral("run-manifest.json"));
        QFile::remove(manifest);
        args << QStringLiteral("--manifest=") + manifest;
    }
    if (!m_output) {
        m_output = new OutputWindow(this);
        connect(m_output, &OutputWindow::finished, this, &MainWindow::generationFinished);
    }
    m_output->open();
    m_output->start(m_probe.program, args, m_runOutputDir);
}

void MainWindow::generationFinished(bool success)
{
    if (!success)
        return;
    const QString manifest = QDir(m_tempDir.path()).filePath(QStringLiteral("run-manifest.json"));
    m_preview->setImages(collectPreviews(m_runOutputDir, manifest),
                         tr("Showing the output in %1").arg(QDir::toNativeSeparators(m_runOutputDir)));
}

void MainWindow::showCommandLine()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("pcb2gcode command line"));
    dialog.resize(760, 300);
    auto *text = new QPlainTextEdit;
    text->setReadOnly(true);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    const QString program = m_probe.program.isEmpty() ? pcb2gcode::executableName() : m_probe.program;
    text->setPlainText(pcb2gcode::shellCommand(program, commandArguments()));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton *copy = buttons->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    connect(copy, &QPushButton::clicked, &dialog, [text] { QGuiApplication::clipboard()->setText(text->toPlainText()); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(text);
    layout->addWidget(buttons);
    dialog.exec();
}

QString MainWindow::configHeader() const
{
    return tr("Configuration file written by pcb2gcodeGUI %1 for pcb2gcode %2 on %3\n"
              "Use it with: pcb2gcode --config=<this file>")
        .arg(QCoreApplication::applicationVersion(), m_probe.schema.pcb2gcodeVersion,
             QDateTime::currentDateTime().toString(Qt::ISODate));
}

bool MainWindow::writeConfig(const QString &path)
{
    QString error;
    if (!m_form->toConfig().save(path, configHeader(), &error)) {
        QMessageBox::warning(this, tr("Save configuration"), tr("Can't save %1: %2").arg(path, error));
        return false;
    }
    return true;
}

void MainWindow::newConfig()
{
    m_form->resetAll(true);
    m_configPath.clear();
    updateTitle();
}

bool MainWindow::openConfig(const QString &path)
{
    QString error;
    if (!QFileInfo(path).isFile()) {
        QMessageBox::warning(this, tr("Open configuration"), tr("%1 doesn't exist.").arg(path));
        return false;
    }
    const ConfigFile file = ConfigFile::load(path, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Open configuration"), tr("Can't open %1: %2").arg(path, error));
        return false;
    }
    const QStringList notes = m_form->applyConfig(file, QFileInfo(path).absolutePath());
    m_configPath = QFileInfo(path).absoluteFilePath();
    m_settings->setValue(QStringLiteral("lastConfigDir"), QFileInfo(path).absolutePath());
    updateTitle();
    showNotes(QFileInfo(path).fileName(), notes);
    return true;
}

void MainWindow::openConfigDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open configuration"), m_settings->value(QStringLiteral("lastConfigDir")).toString(),
        tr("pcb2gcode configuration (millproject *.millproject *.cfg *.txt);;All files (*)"));
    if (!path.isEmpty())
        openConfig(path);
}

bool MainWindow::saveConfig()
{
    if (m_configPath.isEmpty())
        return saveConfigAs();
    return writeConfig(m_configPath);
}

bool MainWindow::saveConfigAs()
{
    const QString start = m_configPath.isEmpty()
                              ? QDir(m_settings->value(QStringLiteral("lastConfigDir")).toString()).filePath("millproject")
                              : m_configPath;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save configuration"), start);
    if (path.isEmpty() || !writeConfig(path))
        return false;
    m_configPath = path;
    m_settings->setValue(QStringLiteral("lastConfigDir"), QFileInfo(path).absolutePath());
    updateTitle();
    return true;
}

QString MainWindow::defaultConfigPath() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QString::fromLatin1(kDefaultConfigName));
}

void MainWindow::saveDefaultConfig()
{
    QDir().mkpath(QFileInfo(defaultConfigPath()).absolutePath());
    if (writeConfig(defaultConfigPath()))
        statusBar()->showMessage(tr("Saved as the default configuration"), 5000);
}

void MainWindow::loadDefaultConfig()
{
    loadStartupConfig();
}

void MainWindow::resetDefaultConfig()
{
    if (QMessageBox::question(this, tr("Reset default configuration"),
                              tr("Forget the saved default configuration and go back to pcb2gcode's defaults?")) !=
        QMessageBox::Yes)
        return;
    QFile::remove(defaultConfigPath());
    loadStartupConfig();
}

void MainWindow::locatePcb2gcode()
{
    const QString start = m_probe.program.isEmpty() ? QDir::homePath() : QFileInfo(m_probe.program).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Locate pcb2gcode"), start);
    if (path.isEmpty())
        return;
    const ConfigFile current = m_form->toConfig();
    m_pcb2gcodePath = path;
    setPcb2gcode(path);
    if (m_probe.program.isEmpty())
        QMessageBox::warning(this, tr("Locate pcb2gcode"), tr("%1 didn't answer --version.").arg(path));
    showNotes(tr("Switching pcb2gcode"), m_form->applyConfig(current));
}

void MainWindow::aboutPcb2gcode()
{
    QMessageBox::about(this, tr("About pcb2gcode"),
                       m_probe.program.isEmpty()
                           ? tr("pcb2gcode wasn't found.")
                           : QStringLiteral("<p><b>pcb2gcode %1</b></p><pre>%2</pre><p><a "
                                            "href='https://github.com/pcb2gcode/pcb2gcode'>github.com/pcb2gcode/"
                                            "pcb2gcode</a></p>")
                                 .arg(m_probe.version, m_probe.versionText.toHtmlEscaped()));
}

void MainWindow::aboutGui()
{
    QMessageBox::about(this, tr("About pcb2gcodeGUI"),
                       tr("<p><b>pcb2gcodeGUI %1</b></p>"
                          "<p>A graphical front end for pcb2gcode. Its options come from pcb2gcode itself.</p>"
                          "<p>Created by Nicola Corna, supported by Enrique Condes and contributors.</p>"
                          "<p><a href='https://github.com/pcb2gcode/pcb2gcodeGUI'>"
                          "github.com/pcb2gcode/pcb2gcodeGUI</a></p>")
                           .arg(QCoreApplication::applicationVersion()));
}

void MainWindow::showNotes(const QString &what, const QStringList &notes)
{
    if (notes.isEmpty())
        return;
    QMessageBox box(QMessageBox::Information, tr("Configuration notes"),
                    tr("%1: %n setting(s) were changed or skipped while loading.", nullptr, int(notes.size()))
                        .arg(what),
                    QMessageBox::Ok, this);
    box.setDetailedText(notes.join('\n'));
    box.exec();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_previewProcess.kill();
    m_previewProcess.waitForFinished(1000);
    saveSettings();
    event->accept();
}
