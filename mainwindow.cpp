/*
 * Copyright (c) 2015-2016 Nicola Corna (nicola@corna.info)
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
#include "ui_mainwindow.h"
#include "cmdlineargs.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDateTime>
#include <QRegularExpression>
#include <QGraphicsSvgItem>

#include <cmath>
#include <algorithm>

#include "settings.h"

const QString MainWindow::names[] = { "File", "Common", "Mill", "Drill", "Outline", "Autoleveller" };

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    pcb2gcodeVersion(getPcb2gcodeVersion()),
    pcb2gcodeProcess(this),
    pcb2gcodeKilled(false),
    changeMetricImperialValues(true),
    scene(this),
    loadingIcon(":/images/loading.gif"),
    imagesFolder(QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                    "/pcb2gcode-" + QString::number(QCoreApplication::applicationPid()))
{
    checkPcb2gcodeVersion();

    QString appDataLocation;

    ui->setupUi(this);

    inputUnits.addButton(ui->inputMetricRadioButton, 0);
    inputUnits.addButton(ui->inputImperialRadioButton, 1);
    outputUnits.addButton(ui->outputMetricRadioButton, 0);
    outputUnits.addButton(ui->outputImperialRadioButton, 1);
    mirrorType.addButton(ui->mirrorAbsoluteOnRadioButton, 0);
    mirrorType.addButton(ui->mirrorAbsoluteOffRadioButton, 1);

    pcb2gcodeProcess.setProcessChannelMode(QProcess::MergedChannels);
    pcb2gcodeImageProcess.setProcessChannelMode(QProcess::MergedChannels);

    QDir().mkdir(imagesFolder);

    args[ FILEARGS ].insert("front", ui->frontLineEdit);
    args[ FILEARGS ].insert("back", ui->backLineEdit);
    args[ FILEARGS ].insert("outline", ui->outlineLineEdit);
    args[ FILEARGS ].insert("drill", ui->drillLineEdit);
    args[ FILEARGS ].insert("preamble", ui->preambleLineEdit);
    args[ FILEARGS ].insert("preamble-text", ui->preambletextLineEdit);
    args[ FILEARGS ].insert("postamble", ui->postambleLineEdit);
    args[ FILEARGS ].insert("output-dir", ui->outputDirLineEdit);

    args[ COMMONARGS ].insert("metric", &inputUnits);
    args[ COMMONARGS ].insert("metricoutput", &outputUnits);
    args[ COMMONARGS ].insert("zsafe", ui->zsafeDoubleSpinBox);
    args[ COMMONARGS ].insert("zchange", ui->zchangeDoubleSpinBox);
    args[ COMMONARGS ].insert("vectorial", ui->vectorialCheckBox);
    args[ COMMONARGS ].insert("nog64", ui->nog64CheckBox);
    args[ COMMONARGS ].insert("tolerance", ui->toleranceDoubleSpinBox);
    args[ COMMONARGS ].insert("optimise", ui->optimiseCheckBox);
    args[ COMMONARGS ].insert("zero-start", ui->zerostartCheckBox);
    args[ COMMONARGS ].insert("mirror-absolute", &mirrorType);
    args[ COMMONARGS ].insert("dpi", ui->dpiSpinBox);
    args[ COMMONARGS ].insert("tile-x", ui->tilexSpinBox);
    args[ COMMONARGS ].insert("tile-y", ui->tileySpinBox);

    args[ MILLARGS ].insert("zwork", ui->zworkDoubleSpinBox);
    args[ MILLARGS ].insert("mill-feed", ui->millfeedSpinBox);
    args[ MILLARGS ].insert("mill-speed", ui->millspeedSpinBox);
    args[ MILLARGS ].insert("offset", ui->offsetDoubleSpinBox);
    args[ MILLARGS ].insert("voronoi", ui->voronoiCheckBox);
    args[ MILLARGS ].insert("extra-passes", ui->extrapassesSpinBox);

    args[ DRILLARGS ].insert("zdrill", ui->zdrillDoubleSpinBox);
    args[ DRILLARGS ].insert("drill-feed", ui->drillfeedSpinBox);
    args[ DRILLARGS ].insert("drill-speed", ui->drillspeedSpinBox);
    args[ DRILLARGS ].insert("milldrill", ui->milldrillCheckBox);
    args[ DRILLARGS ].insert("milldrill-diameter", ui->milldrilldiameterDoubleSpinBox);
    args[ DRILLARGS ].insert("drill-side", ui->drillsideComboBox);
    args[ DRILLARGS ].insert("onedrill", ui->onedrillCheckBox);
    args[ DRILLARGS ].insert("nog81", ui->nog81CheckBox);
    args[ DRILLARGS ].insert("nog91-1", ui->nog911CheckBox);

    args[ OUTLINEARGS ].insert("cutter-diameter", ui->cutterdiameterDoubleSpinBox);
    args[ OUTLINEARGS ].insert("zcut", ui->zcutDoubleSpinBox);
    args[ OUTLINEARGS ].insert("cut-feed", ui->cutfeedSpinBox);
    args[ OUTLINEARGS ].insert("cut-speed", ui->cutspeedSpinBox);
    args[ OUTLINEARGS ].insert("cut-infeed", ui->cutinfeedDoubleSpinBox);
    args[ OUTLINEARGS ].insert("outline-width", ui->outlinewidthDoubleSpinBox);
    args[ OUTLINEARGS ].insert("bridges", ui->bridgesDoubleSpinBox);
    args[ OUTLINEARGS ].insert("zbridges", ui->zbridgesDoubleSpinBox);
    args[ OUTLINEARGS ].insert("bridgesnum", ui->bridgesnumSpinBox);
    args[ OUTLINEARGS ].insert("cut-side", ui->cutsideComboBox);
    args[ OUTLINEARGS ].insert("fill-outline", ui->filloutlineCheckBox);

    args[ AUTOLEVELLERARGS ].insert("al-front", ui->alfrontCheckBox);
    args[ AUTOLEVELLERARGS ].insert("al-back", ui->albackCheckBox);
    args[ AUTOLEVELLERARGS ].insert("software", ui->softwareComboBox);
    args[ AUTOLEVELLERARGS ].insert("al-x", ui->alxDoubleSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-y", ui->alyDoubleSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-probefeed", ui->alprobefeedSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-probe-on", ui->alprobeonLineEdit);
    args[ AUTOLEVELLERARGS ].insert("al-probe-off", ui->alprobeoffLineEdit);
    args[ AUTOLEVELLERARGS ].insert("al-probecode", ui->alprobecodeLineEdit);
    args[ AUTOLEVELLERARGS ].insert("al-probevar", ui->alprobevarSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-setzzero", ui->alsetzzeroLineEdit);

    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionShow_command_line_arguments, SIGNAL(triggered(bool)), this, SLOT(menu_showCommandLineArguments()));
    connect(ui->actionAbout_pcb2gcode, SIGNAL(triggered()), this, SLOT(menu_aboutpcb2gcode()));
    connect(ui->actionAbout_pcb2gcodeGUI, SIGNAL(triggered()), this, SLOT(menu_aboutpcb2gcodeGUI()));
    connect(ui->actionPcb2gcodeManual, SIGNAL(triggered()), this, SLOT(menu_manual()));

    connect(ui->actionSave_configuration_file, SIGNAL(triggered()), this, SLOT(askAndSaveConfFile()));
    connect(ui->actionLoad_configuration_file, SIGNAL(triggered()), this, SLOT(askAndLoadConfFile()));
    connect(ui->actionSave_current_as_default_configuration, SIGNAL(triggered()), this, SLOT(saveDefaultConfFile()));
    connect(ui->actionReset_the_default_configuration, SIGNAL(triggered()), this, SLOT(resetDefaultConfFile()));
    connect(ui->actionLoad_default_configuration, SIGNAL(triggered()), this, SLOT(loadDefaultConfFile()));

    connect(ui->frontPushButton, SIGNAL(clicked()), this, SLOT(getFrontFile()));
    connect(ui->backPushButton, SIGNAL(clicked()), this, SLOT(getBackFile()));
    connect(ui->outlinePushButton, SIGNAL(clicked()), this, SLOT(getOutlineFile()));
    connect(ui->drillPushButton, SIGNAL(clicked()), this, SLOT(getDrillFile()));
    connect(ui->preamblePushButton, SIGNAL(clicked()), this, SLOT(getPreambleFile()));
    connect(ui->preambletextPushButton, SIGNAL(clicked()), this, SLOT(getPreambletextFile()));
    connect(ui->postamblePushButton, SIGNAL(clicked()), this, SLOT(getPostambleFile()));
    connect(ui->outputDirPushButton, SIGNAL(clicked()), this, SLOT(getOutputDirectory()));

    connect(ui->vectorialCheckBox, SIGNAL(toggled(bool)), this, SLOT(vectorialEnable(bool)));
    connect(ui->vectorialCheckBox, SIGNAL(toggled(bool)), this, SLOT(bridgesAvailable()));
    connect(ui->vectorialCheckBox, SIGNAL(toggled(bool)), ui->voronoiCheckBox, SLOT(setEnabled(bool)));
    connect(ui->voronoiCheckBox, SIGNAL(toggled(bool)), this, SLOT(voronoiEnable(bool)));
    connect(ui->filloutlineCheckBox, SIGNAL(toggled(bool)), ui->outlinewidthDoubleSpinBox, SLOT(setEnabled(bool)));
    connect(ui->optimiseCheckBox, SIGNAL(toggled(bool)), this, SLOT(bridgesAvailable()));
    connect(ui->milldrillCheckBox, SIGNAL(toggled(bool)), ui->milldrilldiameterDoubleSpinBox, SLOT(setEnabled(bool)));
    connect(ui->softwareComboBox, SIGNAL(currentTextChanged(QString)), this, SLOT(updateAlCustomEnableState(QString)));

    connect(ui->startPushButton, SIGNAL(clicked()), this, SLOT(startPcb2gcode()));
    connect(ui->inputMetricRadioButton, SIGNAL(toggled(bool)), this, SLOT(changeMetricInputUnits(bool)));

    connect(&pcb2gcodeProcess, SIGNAL(finished(int)), this, SLOT(pcb2gcodeStopped()));
    connect(&pcb2gcodeProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(pcb2gcodeError(QProcess::ProcessError)));
    connect(&pcb2gcodeProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(printOutput()));
    connect(&pcb2gcodeProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(changeKillCloseButtonText(QProcess::ProcessState)));

    connect(ui->actionGenerate_a_preview_now, SIGNAL(triggered(bool)), this, SLOT(generateImages()));
    connect(&pcb2gcodeImageProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(imagesGenerated(int,QProcess::ExitStatus)));
    connect(&pcb2gcodeProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(imagesGenerated(int,QProcess::ExitStatus)));
    connect(ui->imageComboBox, SIGNAL(activated(int)), this, SLOT(imageSelected(int)));

    appDataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if (appDataLocation.isEmpty())
        QMessageBox::information(this, tr("Error"), tr("Can't retrieve standard folder location"));
    else
    {
        loadConfFile(appDataLocation + default_config_filename);

        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, appDataLocation);
    }

    settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "pcb2gcodeGUI", "", this);

    ui->actionAutomatically_generate_previews->setChecked(settings->value("autoPreview", true).toBool());
    lastDir = settings->value("lastDir", QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).toString();
    if (lastDir.isEmpty())
        QMessageBox::information(this, tr("Error"), tr("Can't retrieve home location"));

    this->resize(settings->value("Window/width", this->width()).toInt(),
                 settings->value("Window/height", this->height()).toInt());

    ui->gview->setScene(&scene);
    gview_zoom = new Graphics_view_zoom(ui->gview);

    ui->loadingLabel->setMovie(&loadingIcon);
    ui->loadingLabel->hide();
}

void MainWindow::checkPcb2gcodeVersion()
{
    if (pcb2gcodeVersion.isEmpty())
    {
        QMessageBox *errorBox = new QMessageBox;
        errorBox->critical(this, tr("Unable to run " PCB2GCODE_COMMAND_NAME),
                                 tr("Unable to run " PCB2GCODE_COMMAND_NAME "!\n"
                                    PCB2GCODE_COMMAND_NAME " must be in the PATH or in the same folder of pcb2gcodeGUI."));
        errorBox->setFixedSize(600,200);
        exit(EXIT_FAILURE);
    }
    else
    {
        QStringList versionSplit = pcb2gcodeVersion.split('.');
        QVector<int> versionNumbers;

        foreach (const QString& str, versionSplit)
        {
            versionNumbers.append(str.toInt());
        }

        if (std::lexicographical_compare(versionNumbers.begin(), versionNumbers.end(),
                                          targetVersion.begin(), targetVersion.end()))
        {
            QMessageBox *warningBox = new QMessageBox;
            const QString warningMessage = tr("Warning!\n"
                                              "This version of pcb2gcodeGUI requires " PCB2GCODE_COMMAND_NAME
                                              " v%1.%2.%3, but v%4.%5.%6 has been detected.\n"
                                              "Some features may not work.");

            warningBox->warning(this, tr("Old " PCB2GCODE_COMMAND_NAME " detected"),
                                      warningMessage.arg(targetVersion[0]).arg(targetVersion[1]).arg(targetVersion[2])
                                      .arg(versionNumbers[0]).arg(versionNumbers[1]).arg(versionNumbers[2]),
                                      tr("Got it, let's try anyways"));
            warningBox->setFixedSize(600,200);
        }
    }
}

void MainWindow::vectorialEnable(bool enable)
{
    ui->dpiSpinBox->setEnabled(!enable);

    if (enable)
    {
        if (ui->voronoiCheckBox->isChecked())
            ui->extrapassesSpinBox->setEnabled(false);
    }
    else
        ui->extrapassesSpinBox->setEnabled(true);
}

void MainWindow::voronoiEnable(bool enable)
{
    ui->extrapassesSpinBox->setEnabled(!enable);
    ui->offsetDoubleSpinBox->setEnabled(!enable);
}

void MainWindow::bridgesAvailable()
{
    bool bridgesEnabled = ui->vectorialCheckBox->isChecked() || ui->optimiseCheckBox->isChecked();

    ui->bridgesDoubleSpinBox->setEnabled(bridgesEnabled);
    ui->zbridgesDoubleSpinBox->setEnabled(bridgesEnabled);
    ui->bridgesnumSpinBox->setEnabled(bridgesEnabled);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::getFrontFile()
{
    if (getFilename(ui->frontLineEdit, tr("front file"), gerber_front_file_filter + gerber_file_filter))
        if (ui->actionAutomatically_generate_previews->isChecked())
            generateImages();
}

void MainWindow::getBackFile()
{
    if (getFilename(ui->backLineEdit, tr("back file"), gerber_back_file_filter + gerber_file_filter))
        if (ui->actionAutomatically_generate_previews->isChecked())
            generateImages();
}

void MainWindow::getOutlineFile()
{
    if (getFilename(ui->outlineLineEdit, tr("outline file"), gerber_outline_file_filter + gerber_file_filter))
        if (ui->actionAutomatically_generate_previews->isChecked())
            generateImages();
}

void MainWindow::getDrillFile()
{
    if (getFilename(ui->drillLineEdit, tr("drill file"), excellon_file_filter))
        if (ui->actionAutomatically_generate_previews->isChecked())
            generateImages();
}

void MainWindow::getPreambleFile()
{
    getFilename(ui->preambleLineEdit, tr("preamble file"), gcode_file_filter);
}

void MainWindow::getPreambletextFile()
{
    getFilename(ui->preambletextLineEdit, tr("preamble text file"), text_file_filter);
}

void MainWindow::getPostambleFile()
{
    getFilename(ui->postambleLineEdit, tr("postamble file"), gcode_file_filter);
}

void MainWindow::generateImages()
{
    QStringList arguments;
    bool found_output_dir = false;

    loadingIcon.start();
    ui->loadingLabel->show();

    arguments += getCmdLineArguments();

    for (QStringList::iterator i = arguments.begin(); i != arguments.end(); i++)
    {
        if (i->startsWith("--output-dir"))
        {
            *i = "--output-dir=" + imagesFolder;
            found_output_dir = true;
            break;
        }
    }

    if (!found_output_dir)
        arguments << ("--output-dir=" + imagesFolder);

    arguments << "--no-export" << "--noconfigfile";

    if (pcb2gcodeImageProcess.state() != QProcess::NotRunning)
        pcb2gcodeImageProcess.kill();

    currentImagesFolder = imagesFolder;
    vectorial = ui->vectorialCheckBox->isChecked();
    fillOutline = ui->filloutlineCheckBox->isChecked();
    pcb2gcodeImageProcess.start(PCB2GCODE_EXECUTABLE, arguments, QProcess::ReadOnly);
}

void MainWindow::addImageFile(const QDir& dir, const QString& item, const QString& filename)
{
    foreach (const QFileInfo& image, dir.entryInfoList())
    {
        if (image.baseName().contains(filename))
        {
            imagesFilename.append(image.absoluteFilePath());
            ui->imageComboBox->addItem(item);
            break;
        }
    }
}

void MainWindow::imagesGenerated(int exitCode, QProcess::ExitStatus exitStatus)
{
    loadingIcon.stop();
    ui->loadingLabel->hide();

    if (exitCode == EXIT_SUCCESS && exitStatus == QProcess::NormalExit)
    {
        QDir dir(currentImagesFolder);

        if (vectorial)
            dir.setNameFilters(QStringList() << "*.svg");
        else
            dir.setNameFilters(QStringList() << "*.png");

        dir.setFilter(QDir::Files);

        imagesFilename.clear();
        ui->imageComboBox->clear();

        addImageFile(dir, tr("Processed front"), "processed_front");
        addImageFile(dir, tr("Processed back"), "processed_back");
        addImageFile(dir, tr("Processed outline"), "processed_outline");
        addImageFile(dir, tr("Traced front"), "traced_front");
        addImageFile(dir, tr("Traced back"), "traced_back");
        addImageFile(dir, tr("Masked front"), "masked_front");
        addImageFile(dir, tr("Masked back"), "masked_back");
        addImageFile(dir, tr("Input front"), "original_front");
        addImageFile(dir, tr("Input back"), "original_back");
        addImageFile(dir, tr("Input drill"), "original_drill");
        addImageFile(dir, tr("Input outline"), fillOutline ? "outline_filled" : "original_outline");
    }
    else if (sender() != static_cast<QObject *>(&pcb2gcodeProcess)) //Errors from pcb2gcodeProcess are printed in outputWindow
    {
        QMessageBox::critical(this, "Error",
                                 tr("Error while processing input files (error code ") +
                                 QString::number(exitCode) + ')' +
                                 tr("\n\npcb2gcode output:\n") +
                                 pcb2gcodeImageProcess.readAll());
    }

    if (ui->imageComboBox->count() > 0)
    {
        ui->imageComboBox->setCurrentIndex(0);
        ui->imageComboBox->setEnabled(true);
        imageSelected(0);
    }
    else
        ui->imageComboBox->setEnabled(false);
}

void MainWindow::imageSelected(int index)
{
    if (index >= 0)
        showImage(imagesFilename.at(index));
    else
        showImage("");
}

void MainWindow::showImage(QString image)
{
    const bool wasEmpty = scene.items().isEmpty();

    foreach (QGraphicsItem *item, scene.items())
    {
        scene.removeItem(item);
        delete item;
    }

    if (!image.isEmpty())
    {
        if (image.endsWith(".svg"))
            scene.addItem(new QGraphicsSvgItem(image));
        else
            scene.addItem(new QGraphicsPixmapItem(QPixmap(image)));

        if (wasEmpty)
            ui->gview->fitInView(scene.sceneRect(), Qt::KeepAspectRatio);
    }
}

bool MainWindow::getFilename(QLineEdit *saveTo, const QString name, QString filter)
{
    QString filename;

    filename = QFileDialog::getOpenFileName(this, tr("Select the ") + name, lastDir, filter );

    if( filename.isEmpty() )
        return false;
    else
    {
        lastDir = QFileInfo(filename).path();
        saveTo->setText( filename );
        return true;
    }
}

void MainWindow::getOutputDirectory()
{
    QString dirname;

    dirname = QFileDialog::getExistingDirectory(this, tr("Select the output directory"), lastDir );
    if( !dirname.isEmpty() )
    {
        lastDir = dirname;
        ui->outputDirLineEdit->setText( dirname );
    }
}

void MainWindow::changeMetricInputUnits(bool metric)
{
    QDoubleSpinBox *doubleSpinBoxes[] = { ui->zworkDoubleSpinBox, ui->zsafeDoubleSpinBox, ui->offsetDoubleSpinBox,
                                                  ui->zdrillDoubleSpinBox, ui->zchangeDoubleSpinBox, ui->cutterdiameterDoubleSpinBox,
                                                  ui->zcutDoubleSpinBox, ui->cutinfeedDoubleSpinBox, ui->outlinewidthDoubleSpinBox,
                                                  ui->bridgesDoubleSpinBox, ui->zbridgesDoubleSpinBox, ui->alxDoubleSpinBox,
                                                  ui->alyDoubleSpinBox, ui->toleranceDoubleSpinBox };

    QSpinBox *spinBoxes[] = { ui->millfeedSpinBox, ui->drillfeedSpinBox, ui->cutfeedSpinBox, ui->alprobefeedSpinBox };

    const unsigned int doubleSpinBoxesLen =  sizeof(doubleSpinBoxes) / sizeof(doubleSpinBoxes[0]);
    const unsigned int spinBoxesLen =  sizeof(spinBoxes) / sizeof(spinBoxes[0]);
    const double cfactor = metric ? 25.4 : 1/25.4;
    const char *distance = metric ? " mm" : " in" ;
    const char *speed = metric ? " mm/min" : " in/min" ;

    for( unsigned int i = 0; i < doubleSpinBoxesLen; i++ )
        adjustMetricImperial( doubleSpinBoxes[i], cfactor, distance );

    for( unsigned int i = 0; i < spinBoxesLen; i++ )
        adjustMetricImperial( spinBoxes[i], cfactor, speed );
}

void MainWindow::adjustMetricImperial(QSpinBox *spinBox, const double cfactor, const QString suffix)
{
    int value;
    int maximum;
    int minimum;

    if( changeMetricImperialValues )
    {
        value = spinBox->value();
        maximum = spinBox->maximum();
        minimum = spinBox->minimum();

        spinBox->setMaximum( round( maximum * cfactor ) );
        spinBox->setMinimum( round( minimum * cfactor ) );
        spinBox->setValue( round( value * cfactor ) );
    }

    spinBox->setSuffix(suffix);
}

void MainWindow::adjustMetricImperial(QDoubleSpinBox *doubleSpinBox, const double cfactor, const QString suffix)
{
    double value;
    double maximum;
    double minimum;

    if( changeMetricImperialValues )
    {
        if (cfactor < 1)
            doubleSpinBox->setDecimals(doubleSpinBox->decimals() + 1);

        value = doubleSpinBox->value();
        maximum = doubleSpinBox->maximum();
        minimum = doubleSpinBox->minimum();

        doubleSpinBox->setMaximum( maximum * cfactor );
        doubleSpinBox->setMinimum( minimum * cfactor );
        doubleSpinBox->setValue( value * cfactor );

        if (cfactor >= 1)
            doubleSpinBox->setDecimals(doubleSpinBox->decimals() - 1);
    }

    doubleSpinBox->setSuffix(suffix);
}

QStringList MainWindow::getCmdLineArguments()
{
    QStringList arguments;

    arguments += args[ FILEARGS ].getAllArgs("--", false);
    arguments += args[ COMMONARGS ].getAllArgs("--", false);

    if( !ui->frontLineEdit->text().isEmpty() || !ui->backLineEdit->text().isEmpty() )
        arguments += args[ MILLARGS ].getAllArgs("--", false);

    if( !ui->drillLineEdit->text().isEmpty() )
        arguments += args[ DRILLARGS ].getAllArgs("--", false);

    if( !ui->outlineLineEdit->text().isEmpty() || ui->milldrillCheckBox->isChecked() )
        arguments += args[ OUTLINEARGS ].getAllArgs("--", false);

    if ( (ui->alfrontCheckBox->isChecked() || ui->albackCheckBox->isChecked()) &&
         (!ui->frontLineEdit->text().isEmpty() || !ui->backLineEdit->text().isEmpty()) )
        arguments += args[ AUTOLEVELLERARGS ].getAllArgs("--", false);

    return arguments;
}

void MainWindow::startPcb2gcode()
{
    QStringList arguments;

    if (pcb2gcodeImageProcess.state() != QProcess::NotRunning)
        pcb2gcodeImageProcess.kill();

    if( ui->outputDirLineEdit->text().isEmpty() )
        getOutputDirectory();

    if( !ui->outputDirLineEdit->text().isEmpty() )
    {
        loadingIcon.start();
        ui->loadingLabel->show();

        ui->startPushButton->setEnabled(false);

        arguments << "--noconfigfile";
        arguments += getCmdLineArguments();

        pcb2gcodeOutputWindow = new outputWindow(this);
        pcb2gcodeOutputWindow->setWindowTitle(PCB2GCODE_COMMAND_NAME " output");
        pcb2gcodeOutputWindow->setWindowFlags( pcb2gcodeOutputWindow->windowFlags() &
                                               ~( Qt::WindowContextHelpButtonHint | Qt::WindowCloseButtonHint ) );
        connect(pcb2gcodeOutputWindow, SIGNAL(finished(int)), this, SLOT(enableStartButton()));
        pcb2gcodeOutputWindow->show();

        killClosePushButton = pcb2gcodeOutputWindow->getPushButton();
        connect(killClosePushButton, SIGNAL(clicked()), this, SLOT(killCloseButtonClicked()));

        outputTextEdit = pcb2gcodeOutputWindow->getOutputPlainTextEdit();

        currentImagesFolder = ui->outputDirLineEdit->text();
        vectorial = ui->vectorialCheckBox->isChecked();
        fillOutline = ui->filloutlineCheckBox->isChecked();
        pcb2gcodeProcess.start(PCB2GCODE_EXECUTABLE, arguments, QProcess::ReadOnly);
    }
}

void MainWindow::printOutput()
{
    outputTextEdit->insertPlainText(pcb2gcodeProcess.readAllStandardOutput());
    outputTextEdit->moveCursor(QTextCursor::End);
}

void MainWindow::pcb2gcodeError(QProcess::ProcessError error)
{
    if(pcb2gcodeKilled)
    {
        pcb2gcodeKilled = false;
        outputTextEdit->appendHtml(tr("<font color=\"Red\">"
                                      "pcb2gcode has been killed.</font>"));
    }
    else
        switch(error)
        {
            case QProcess::FailedToStart:
                outputTextEdit->appendHtml(tr("<font color=\"Red\">"
                                              "pcb2gcode failed to start. "
                                              "Either the pcb2gcode is missing, "
                                              "or you may have insufficient permissions "
                                              "to invoke the program.</font>"));
                break;

            case QProcess::Crashed:
                outputTextEdit->appendHtml(tr("<font color=\"Red\">"
                                              "pcb2gcode unexpectedly crashed. "
                                              "Please report this error to the project manager."
                                              "</font>"));
                break;

            case QProcess::ReadError:
                outputTextEdit->appendHtml(tr("<font color=\"Red\">"
                                              "An error occured while reading from pcb2gcode."
                                              "Please report this error to the project manager."
                                              "</font>"));
                break;

            default:
               outputTextEdit->appendHtml(tr("<font color=\"Red\">"
                                             "Unspecified error while trying to run pcb2gcode."
                                             "Please report this error to the project manager."
                                             "</font>"));
        }

    pcb2gcodeStopped();
}

void MainWindow::pcb2gcodeStopped()
{
    killClosePushButton->setText(tr("Close window"));
}

void MainWindow::changeKillCloseButtonText(QProcess::ProcessState state)
{
    if( state == QProcess::NotRunning )
        killClosePushButton->setText(tr("Close window"));
    else
        killClosePushButton->setText(tr("Kill " PCB2GCODE_COMMAND_NAME));
}

void MainWindow::killCloseButtonClicked()
{
    outputTextEdit->appendPlainText(QString::number(pcb2gcodeProcess.state()));

    if( pcb2gcodeProcess.state() == QProcess::NotRunning )
        pcb2gcodeOutputWindow->close();
    else
    {
        pcb2gcodeKilled = true;
        pcb2gcodeProcess.kill();
    }
}

void MainWindow::enableStartButton()
{
    ui->startPushButton->setEnabled(true);
}

void MainWindow::menu_showCommandLineArguments()
{
    CmdLineArgs *window = new CmdLineArgs(this);
    QStringList arguments;
    QString arguments_formatted = PCB2GCODE_COMMAND_NAME;

    arguments << "--noconfigfile";
    arguments += getCmdLineArguments();

    for(QStringList::iterator iter = arguments.begin(); iter != arguments.end(); iter++) {
        int pos = iter->indexOf('=');

        if( pos > 0 ) {
            if(iter->contains(' '))
                arguments_formatted += iter->left( pos + 1 ) + '\"' + iter->right( iter->length() - pos - 1 ) + "\" ";
            else
                arguments_formatted += ' ' + *iter;
        }
    }

    window->setText(arguments_formatted);
    window->show();
}

void MainWindow::menu_aboutpcb2gcode()
{
    QMessageBox msgBox(this);

    msgBox.setWindowTitle(tr("About pcb2gcode"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(QString(tr(about_pcb2gcode_str)).arg(QString(pcb2gcodeVersion)));
    msgBox.show();
    msgBox.exec();
}

QString MainWindow::getPcb2gcodeVersion()
{
    QProcess pcb2gcodeVersionProcess(this);

    pcb2gcodeVersionProcess.start(PCB2GCODE_EXECUTABLE, QStringList("--version"), QProcess::ReadOnly);
    pcb2gcodeVersionProcess.waitForFinished(500);

    QRegularExpressionMatch res = QRegularExpression("\\d+\\.\\d+\\.\\d+").match(pcb2gcodeVersionProcess.readAllStandardOutput());

    if(res.hasMatch())
        return res.captured();
    else
        return "";
}

void MainWindow::menu_aboutpcb2gcodeGUI()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("About pcb2gcodeGUI"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText( QString(about_pcb2gcodegui_str).arg(PCB2GCODE_GUI_VERSION) );
    msgBox.exec();
}

void MainWindow::menu_manual()
{
    QDesktopServices::openUrl(QUrl(manual_link, QUrl::StrictMode));
}

void MainWindow::askAndLoadConfFile()
{
    QString filename;

    filename = QFileDialog::getOpenFileName(this, tr("Select a configuration file"), lastDir );
    if( !filename.isEmpty() )
    {
        lastDir = QFileInfo(filename).path();
        if( !loadConfFile(filename) )
            QMessageBox::information(this, tr("Error"), tr("The selected file can't be opened"));
    }
}

bool MainWindow::loadConfFile(const QString filename)
{
    QFile confFile (this);
    QString currentLine;
    int equalPosition;
    QString key;
    QString value;
    bool result;
    bool enabledOption;

    confFile.setFileName(filename);
    confFile.open(QFile::ReadOnly);
    if(confFile.isOpen())
    {
        changeMetricImperialValues = false;
        while( !confFile.atEnd() )
        {
            enabledOption = true;
            currentLine = confFile.readLine();
            currentLine.remove(' ');

            if( currentLine.startsWith("#@#") )
            {
                currentLine.remove(0, 3);       //A line starting with extra_options_prefix is valid
                enabledOption = false;
            }

            if( !currentLine.startsWith('#') && currentLine.contains('=') )  //Ignore comments and lines without '='
            {
                equalPosition = currentLine.indexOf('=');
                key = currentLine.left( equalPosition );
                value = currentLine.right( currentLine.size() - equalPosition - 1 );
                value.chop(1);      //Chop the last character ('/n')

                if (key == "drill-front")
                {
                    key = "drill-side";
                    if(value == "1" || value.compare("true", Qt::CaseInsensitive) == 0)
                        value = "front";
                    else
                        value = "back";
                }

                if (key == "cut-front")
                {
                    key = "cut-side";
                    if(value == "1" || value.compare("true", Qt::CaseInsensitive) == 0)
                        value = "front";
                    else
                        value = "back";
                }

                result = false;
                for(int i = COMMONARGS; i <= AUTOLEVELLERARGS && !result; i++)
                {
                    result = args[i].setValue(key, value);
                    if (result)
                        args[i].setEnabled(key, enabledOption);
                }

                if(result == false)
                    QMessageBox::information(this, tr("Error"), tr("Invalid parameter in configuration file: key=") + key + tr(" value=") + value);
            }
        }

        changeMetricImperialValues = true;
        return true;
    }
    else
        return false;
}

void MainWindow::askAndSaveConfFile()
{
    QString filename;

    filename = QFileDialog::getSaveFileName(this, tr("Save configuration file"), lastDir);
    if( !filename.isEmpty() )
    {
        lastDir = QFileInfo(filename).path();
        saveConfFile(filename);
    }
}

void MainWindow::saveConfFile(const QString filename)
{
    QFile confFile (this);
    QStringList arguments;

    confFile.setFileName(filename);
    confFile.open(QFile::WriteOnly);
    if(confFile.isOpen())
    {
        confFile.write( QString("# Configuration file generated by pcb2gcodeGUI version " PCB2GCODEGUI_VERSION " on " +
                                QDateTime::currentDateTime().toString() + "\n\n" ).toLatin1() );

        for( int i = COMMONARGS; i <= AUTOLEVELLERARGS; i++ )
        {
            arguments = args[i].getAllArgs("", true);
            confFile.write( QString("# " + names[i] + " options\n").toLatin1() );

            for( QStringList::const_iterator j = arguments.begin(); j != arguments.constEnd(); j++ )
                confFile.write( (*j + '\n').toLatin1() );

            confFile.write("\n");
        }

        confFile.close();
    }
    else
        QMessageBox::information(this, tr("Error"), tr("Can't save the file ") + filename);
}

void MainWindow::saveDefaultConfFile()
{
    QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    if( appDataLocation.isEmpty() )
        QMessageBox::information(this, tr("Error"), tr("Can't retrieve standard folder location"));
    else
        if( QDir().mkpath(appDataLocation) )
            saveConfFile(appDataLocation + default_config_filename);
        else
            QMessageBox::information(this, tr("Error"), tr("Can't create path ") + appDataLocation);
}

void MainWindow::loadDefaultConfFile()
{
    QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if( appDataLocation.isEmpty() )
        QMessageBox::information(this, tr("Error"), tr("Can't retrieve standard folder location"));
    else
        if(QFile::exists(appDataLocation + default_config_filename))
            loadConfFile(appDataLocation + default_config_filename);
        else
            ui->setupUi(this);
}

void MainWindow::resetDefaultConfFile()
{
    QString appDataLocation;

    if( QMessageBox::question(this, tr("Reset default configuration"),
                          tr("Are you sure you want to reset the default configuration?"),
                          QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes )
    {
        appDataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
        if( appDataLocation.isEmpty() )
            QMessageBox::information(this, tr("Error"), tr("Can't retrieve standard folder location"));
        else
            if( QFile::exists(appDataLocation + default_config_filename) &&
                !QFile::remove(appDataLocation + default_config_filename) )
                QMessageBox::information(this, tr("Error"), tr("Can't delete default configuration file ")
                                         + appDataLocation + default_config_filename);
            else
                loadDefaultConfFile();
    }
}

void MainWindow::updateAlCustomEnableState(QString text)
{
    bool enabled = text == "custom";

    ui->alprobecodeLineEdit->setEnabled(enabled);
    ui->alprobevarSpinBox->setEnabled(enabled);
    ui->alsetzzeroLineEdit->setEnabled(enabled);
}

void MainWindow::clearImages()
{
    QDir dir(imagesFolder);

    dir.setNameFilters(QStringList() << "*.*");
    dir.setFilter(QDir::Files);

    foreach (const QString& dirFile, dir.entryList())
    {
        dir.remove(dirFile);
    }
}

void MainWindow::closeEvent(QCloseEvent *)
{
    clearImages();
    QDir().rmdir(imagesFolder);

    settings->setValue("autoPreview", ui->actionAutomatically_generate_previews->isChecked());
    settings->setValue("lastDir", lastDir);

    settings->setValue("Window/width", this->width());
    settings->setValue("Window/height", this->height());
}
