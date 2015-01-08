/*
 * Copyright (c) 2015 Nicola Corna (nicola@corna.info)
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
#include "settings.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDateTime>

const QString MainWindow::names[] = { "File", "Common", "Mill", "Drill", "Outline", "Autoleveller" };

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    inputSystemOfMeasurement(this),
    outputSystemOfMeasurement(this),
    pcb2gcodeProcess(this),
    pcb2gcodeKilled(false),
    changeMetricImperialValues(true)
{
    ui->setupUi(this);
    this->setFixedSize(this->width(), this->height());

    inputSystemOfMeasurement.addButton(ui->inputMetricRadioButton, METRIC);
    inputSystemOfMeasurement.addButton(ui->inputImperialRadioButton, IMPERIAL);
    outputSystemOfMeasurement.addButton(ui->outputMetricRadioButton, METRIC);
    outputSystemOfMeasurement.addButton(ui->outputImperialRadioButton, IMPERIAL);
    inputSystemOfMeasurement.button(METRIC)->setChecked(true);
    outputSystemOfMeasurement.button(METRIC)->setChecked(true);

    args[ FILEARGS ].insert("front", ui->frontLineEdit);
    args[ FILEARGS ].insert("back", ui->backLineEdit);
    args[ FILEARGS ].insert("outline", ui->outlineLineEdit);
    args[ FILEARGS ].insert("drill", ui->drillLineEdit);
    args[ FILEARGS ].insert("preamble", ui->preambleLineEdit);
    args[ FILEARGS ].insert("preamble-text", ui->preambletextLineEdit);
    args[ FILEARGS ].insert("postamble", ui->postambleLineEdit);

    args[ COMMONARGS ].insert("metric", &inputSystemOfMeasurement);
    args[ COMMONARGS ].insert("metricoutput", &outputSystemOfMeasurement);
    args[ COMMONARGS ].insert("zsafe", ui->zsafeDoubleSpinBox);
    args[ COMMONARGS ].insert("zchange", ui->zchangeDoubleSpinBox);
    args[ COMMONARGS ].insert("g64", ui->g64DoubleDoubleSpinBox);
    args[ COMMONARGS ].insert("g64enable", ui->g64CheckBox, true);
    args[ COMMONARGS ].insert("optimise", ui->optimiseCheckBox);
    args[ COMMONARGS ].insert("mirror-absolute", ui->mirrorabsoluteCheckBox);
    args[ COMMONARGS ].insert("svg", ui->svgCheckBox);
    args[ COMMONARGS ].insert("dpi", ui->dpiSpinBox);

    args[ MILLARGS ].insert("zwork", ui->zworkDoubleSpinBox);
    args[ MILLARGS ].insert("mill-feed", ui->millfeedSpinBox);
    args[ MILLARGS ].insert("mill-speed", ui->millspeedSpinBox);
    args[ MILLARGS ].insert("offset", ui->offsetDoubleSpinBox);
    args[ MILLARGS ].insert("extra-passes", ui->extraassesSpinBox);

    args[ DRILLARGS ].insert("zdrill", ui->zdrillDoubleSpinBox);
    args[ DRILLARGS ].insert("drill-feed", ui->drillfeedSpinBox);
    args[ DRILLARGS ].insert("drill-speed", ui->drillspeedSpinBox);
    args[ DRILLARGS ].insert("milldrill", ui->milldrillCheckBox);
    args[ DRILLARGS ].insert("drill-front", ui->drillfrontCheckBox);
    args[ DRILLARGS ].insert("onedrill", ui->onedrillCheckBox);

    args[ OUTLINEARGS ].insert("cutter-diameter", ui->cutterdiameterDoubleSpinBox);
    args[ OUTLINEARGS ].insert("zcut", ui->zcutDoubleSpinBox);
    args[ OUTLINEARGS ].insert("cut-feed", ui->cutfeedSpinBox);
    args[ OUTLINEARGS ].insert("cut-speed", ui->cutspeedSpinBox);
    args[ OUTLINEARGS ].insert("cut-infeed", ui->cutinfeedDoubleSpinBox);
    args[ OUTLINEARGS ].insert("outline-width", ui->outlinewidthDoubleSpinBox);
    args[ OUTLINEARGS ].insert("bridges", ui->bridgesDoubleSpinBox);
    args[ OUTLINEARGS ].insert("zbridges", ui->zbridgesDoubleSpinBox);
    args[ OUTLINEARGS ].insert("cut-front", ui->cutfrontCheckBox);
    args[ OUTLINEARGS ].insert("fill-outline", ui->filloutlineCheckBox);

    args[ AUTOLEVELLERARGS ].insert("al-front", ui->alfrontCheckBox);
    args[ AUTOLEVELLERARGS ].insert("al-back", ui->albackCheckBox);
    args[ AUTOLEVELLERARGS ].insert("software", ui->softwareComboBox);
    args[ AUTOLEVELLERARGS ].insert("al-x", ui->alxDoubleSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-y", ui->alyDoubleSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-probefeed", ui->alprobefeedDoubleSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-probeon", ui->alprobeonLineEdit);
    args[ AUTOLEVELLERARGS ].insert("al-probeoff", ui->alprobeoffLineEdit);

    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionAbout_pcb2gcode, SIGNAL(triggered()), this, SLOT(menu_aboutpcb2gcode()));
    connect(ui->actionAbout_pcb2gcodeGUI, SIGNAL(triggered()), this, SLOT(menu_aboutpcb2gcodeGUI()));
    connect(ui->actionManual, SIGNAL(triggered()), this, SLOT(menu_manual()));

    connect(ui->actionSave_configuration_file, SIGNAL(triggered()), this, SLOT(askAndSaveConfFile()));
    connect(ui->actionOpen_configuration_file, SIGNAL(triggered()), this, SLOT(askAndLoadConfFile()));
    connect(ui->actionSave_current_as_default_configuration, SIGNAL(triggered()), this, SLOT(saveDefaultConfFile()));
    connect(ui->actionReset_the_default_configuration, SIGNAL(triggered()), this, SLOT(resetDefaultConfFile()));

    connect(ui->frontPushButton, SIGNAL(clicked()), this, SLOT(getFrontFile()));
    connect(ui->backPushButton, SIGNAL(clicked()), this, SLOT(getBackFile()));
    connect(ui->outlinePushButton, SIGNAL(clicked()), this, SLOT(getOutlineFile()));
    connect(ui->drillPushButton, SIGNAL(clicked()), this, SLOT(getDrillFile()));
    connect(ui->preamblePushButton, SIGNAL(clicked()), this, SLOT(getPreambleFile()));
    connect(ui->preambletextPushButton, SIGNAL(clicked()), this, SLOT(getPreambletextFile()));
    connect(ui->postamblePushButton, SIGNAL(clicked()), this, SLOT(getPostambleFile()));
    connect(ui->outputDirPushButton, SIGNAL(clicked()), this, SLOT(getOutputDirectory()));

    connect(ui->g64CheckBox, SIGNAL(toggled(bool)), ui->g64DoubleDoubleSpinBox, SLOT(setEnabled(bool)));
    connect(ui->filloutlineCheckBox, SIGNAL(toggled(bool)), ui->outlinewidthDoubleSpinBox, SLOT(setEnabled(bool)));
    connect(ui->startPushButton, SIGNAL(clicked()), this, SLOT(startPcb2gcode()));
    connect(ui->inputMetricRadioButton, SIGNAL(toggled(bool)), this, SLOT(changeMetricInputUnits(bool)));

    connect(&pcb2gcodeProcess, SIGNAL(finished(int)), this, SLOT(pcb2gcodeStopped()));
    connect(&pcb2gcodeProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(pcb2gcodeError(QProcess::ProcessError)));
    connect(&pcb2gcodeProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(printOutput()));
    connect(&pcb2gcodeProcess, SIGNAL(readyReadStandardError()), this, SLOT(printOutput()));
    connect(&pcb2gcodeProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(changeKillCloseButtonText(QProcess::ProcessState)));

    loadDefaultConfFile();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::getFrontFile()
{
    getFilename(ui->frontLineEdit, "front file");
}

void MainWindow::getBackFile()
{
    getFilename(ui->backLineEdit, "back file");
}

void MainWindow::getOutlineFile()
{
    getFilename(ui->outlineLineEdit, "outline file");
}

void MainWindow::getDrillFile()
{
    getFilename(ui->drillLineEdit, "drill file");
}

void MainWindow::getPreambleFile()
{
    getFilename(ui->preambleLineEdit, "preamble file");
}

void MainWindow::getPreambletextFile()
{
    getFilename(ui->preambletextLineEdit, "preamble text file");
}

void MainWindow::getPostambleFile()
{
    getFilename(ui->postambleLineEdit, "postamble file");
}

void MainWindow::getFilename(QLineEdit *saveTo, QString name)
{
    QString filename;

    filename = QFileDialog::getOpenFileName(this, "Select the" + name, lastDir );
    if( !filename.isEmpty() )
        saveTo->setText( filename );
}

void MainWindow::getOutputDirectory()
{
    QString dirname;

    dirname = QFileDialog::getExistingDirectory(this, "Select the output directory", lastDir );
    if( !dirname.isEmpty() )
        ui->outputDirLineEdit->setText( dirname );
}

void MainWindow::changeMetricInputUnits(bool metric)
{
    int index;
    double cfactor;
    const char *distance[2] = { " mm", " in" };
    const char *speedMin[2] = { " mm/min", " in/min" };
    const char *speedSec[2] = { " mm/s", " in/s" };

    QDoubleSpinBox *doubleSpinBoxes[] = { ui->zworkDoubleSpinBox, ui->zsafeDoubleSpinBox,ui->offsetDoubleSpinBox, ui->zdrillDoubleSpinBox,
                                        ui->zchangeDoubleSpinBox, ui->cutterdiameterDoubleSpinBox, ui->zcutDoubleSpinBox,
                                        ui->cutinfeedDoubleSpinBox, ui->outlinewidthDoubleSpinBox, ui->bridgesDoubleSpinBox,
                                        ui->zbridgesDoubleSpinBox, ui->alxDoubleSpinBox, ui->alyDoubleSpinBox,
                                        ui->g64DoubleDoubleSpinBox };

    const unsigned int doubleSpinBoxesLen =  sizeof(doubleSpinBoxes) / sizeof(doubleSpinBoxes[0]);

    if (metric)
    {
        index = 0;
        cfactor = 1/2.54;
    }
    else
    {
        index = 1;
        cfactor = 2.54;
    }

    for( unsigned int i = 0; i < doubleSpinBoxesLen; i++ )
        adjustMetricImperial( doubleSpinBoxes[i], cfactor, distance[index] );

    adjustMetricImperial( ui->millfeedSpinBox, cfactor, speedMin[index] );
    adjustMetricImperial( ui->drillfeedSpinBox, cfactor, speedMin[index] );
    adjustMetricImperial( ui->cutfeedSpinBox, cfactor, speedMin[index] );
    adjustMetricImperial( ui->alprobefeedDoubleSpinBox, cfactor, speedSec[index] );
}

void MainWindow::adjustMetricImperial(QSpinBox *spinBox, const double cfactor, const QString suffix)
{
    int value = spinBox->value();
    int maximum = spinBox->maximum();
    int minimum = spinBox->minimum();

    spinBox->setMaximum( round( maximum * cfactor ) );
    spinBox->setMinimum( round( minimum * cfactor ) );
    spinBox->setValue( round( value * cfactor ) );

    spinBox->setSuffix(suffix);
}

void MainWindow::adjustMetricImperial(QDoubleSpinBox *doubleSpinBox, const double cfactor, const QString suffix)
{
    double value;
    double maximum;
    double minimum;

    if( changeMetricImperialValues )
    {
        value = doubleSpinBox->value();
        maximum = doubleSpinBox->maximum();
        minimum = doubleSpinBox->minimum();

        doubleSpinBox->setMaximum( maximum * cfactor );
        doubleSpinBox->setMinimum( minimum * cfactor );
        doubleSpinBox->setValue( value * cfactor );
    }

    doubleSpinBox->setSuffix(suffix);
}

QStringList MainWindow::getCmdLineArguments()
{
    QStringList arguments;
    int i;

    arguments += args[ FILEARGS ].getAllArgs();
    arguments += args[ COMMONARGS ].getAllArgs();

    if( !ui->frontLineEdit->text().isEmpty() || !ui->backLineEdit->text().isEmpty() )
        arguments += args[ MILLARGS ].getAllArgs();

    if( !ui->drillLineEdit->text().isEmpty() )
        arguments += args[ DRILLARGS ].getAllArgs();

    if( !ui->outlineLineEdit->text().isEmpty() )
        arguments += args[ OUTLINEARGS ].getAllArgs();

    if ( (ui->alfrontCheckBox->isChecked() || ui->albackCheckBox->isChecked()) &&
         (!ui->frontLineEdit->text().isEmpty() || !ui->backLineEdit->text().isEmpty()) )
        arguments += args[ AUTOLEVELLERARGS ].getAllArgs();

    i = 0;

    while( i < arguments.size() )
    {
        if( arguments[i].contains("=false") )   //Remove lines with a false boolean option ("--metric=false")
            arguments.removeAt(i);
        else
        {
            arguments[i].remove("=true");   //Remove =true ("--metric=true" --> "--metric")
            arguments[i].prepend("--");     //Add the "--"
            i++;
        }
    }

    return arguments;
}

void MainWindow::startPcb2gcode()
{
    QStringList arguments;

    if( ui->outputDirLineEdit->text().isEmpty() )
        getOutputDirectory();

    if( !ui->outputDirLineEdit->text().isEmpty() )
    {
        ui->startPushButton->setEnabled(false);

        arguments = getCmdLineArguments();

        pcb2gcodeOutputWindow = new outputWindow(this);
        pcb2gcodeOutputWindow->setWindowTitle(PCB2GCODE_COMMAND_NAME " output");
        pcb2gcodeOutputWindow->setWindowFlags( pcb2gcodeOutputWindow->windowFlags() &
                                               ~( Qt::WindowContextHelpButtonHint | Qt::WindowCloseButtonHint ) );
        connect(pcb2gcodeOutputWindow, SIGNAL(finished(int)), this, SLOT(enableStartButton()));
        pcb2gcodeOutputWindow->show();

        killClosePushButton = pcb2gcodeOutputWindow->getPushButton();
        connect(killClosePushButton, SIGNAL(clicked()), this, SLOT(killCloseButtonClicked()));

        outputTextEdit = pcb2gcodeOutputWindow->getPlainTextEdit();
        outputTextEdit->appendPlainText(QString("Starting ") + PCB2GCODE_EXECUTABLE + ' ' + arguments.join(' ')) ;

        pcb2gcodeProcess.setWorkingDirectory(ui->outputDirLineEdit->text());
        pcb2gcodeProcess.start(PCB2GCODE_EXECUTABLE, arguments, QProcess::ReadOnly);
    }
}

void MainWindow::printOutput()
{
    outputTextEdit->appendHtml(QString("<font color=\"Red\">") + pcb2gcodeProcess.readAllStandardError() + "</font>");
    outputTextEdit->appendPlainText(pcb2gcodeProcess.readAllStandardOutput());
}

void MainWindow::pcb2gcodeError(QProcess::ProcessError error)
{
    if(pcb2gcodeKilled)
    {
        pcb2gcodeKilled = false;
        outputTextEdit->appendHtml("<font color=\"Red\">"
                                    "pcb2gcode has been killed.</font>");
    }
    else
        switch(error)
        {
            case QProcess::FailedToStart:
                outputTextEdit->appendHtml("<font color=\"Red\">"
                                            "pcb2gcode failed to start. "
                                            "Either the pcb2gcode is missing, "
                                            "or you may have insufficient permissions "
                                            "to invoke the program.</font>");
                break;

            case QProcess::Crashed:
                outputTextEdit->appendHtml("<font color=\"Red\">"
                                            "pcb2gcode unexpectedly crashed. "
                                            "Please report this error to the project manager."
                                            "</font>");
                break;

            case QProcess::ReadError:
                outputTextEdit->appendHtml("<font color=\"Red\">"
                                            "An error occured while reading from pcb2gcode."
                                            "Please report this error to the project manager."
                                            "</font>");
                break;

            default:
               outputTextEdit->appendHtml("<font color=\"Red\">"
                                            "Unspecified error while trying to run pcb2gcode."
                                            "Please report this error to the project manager."
                                            "</font>");
        }

    pcb2gcodeStopped();
}

void MainWindow::pcb2gcodeStopped()
{
    killClosePushButton->setText("Close window");
}

void MainWindow::changeKillCloseButtonText(QProcess::ProcessState state)
{
    if( state == QProcess::NotRunning )
        killClosePushButton->setText("Close window");
    else
        killClosePushButton->setText("Kill " PCB2GCODE_COMMAND_NAME);
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

void MainWindow::menu_aboutpcb2gcode()
{
    QMessageBox msgBox(this);
    QProcess pcb2gcodeVersionProcess(this);
    QByteArray version;

    msgBox.setWindowTitle("About pcb2gcode");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText( QString(about_pcb2gcode_str).arg("(retrieving version...)") );
    msgBox.show();

    pcb2gcodeVersionProcess.start(PCB2GCODE_EXECUTABLE, QStringList("--version"), QProcess::ReadWrite);
    pcb2gcodeVersionProcess.waitForReadyRead(5000);
    version = pcb2gcodeVersionProcess.readAllStandardOutput();

    if( version.isEmpty() )
        msgBox.setText( QString(about_pcb2gcode_str).arg("(<font color=\"Red\">can't run" PCB2GCODE_EXECUTABLE "</font>)") );
    else
        msgBox.setText( QString(about_pcb2gcode_str).arg(QString(version)) );

    msgBox.exec();
}

void MainWindow::menu_aboutpcb2gcodeGUI()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About pcb2gcodeGUI");
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

    filename = QFileDialog::getOpenFileName(this, "Select a configuration file", lastDir );
    if( !filename.isEmpty() )
        if( !loadConfFile(filename) )
            QMessageBox::information(this, "Error", "The selected file can't be opened");
}

bool MainWindow::loadConfFile(QString filename)
{
    QFile confFile (this);
    QString currentLine;
    int equalPosition;
    QString key;
    QString value;
    bool result;

    confFile.setFileName(filename);
    confFile.open(QFile::ReadOnly);
    if(confFile.isOpen())
    {
        changeMetricImperialValues = false;
        while( !confFile.atEnd() )
        {
            currentLine = confFile.readLine();
            currentLine.remove(' ');

            if( currentLine.startsWith("#@!") )
                currentLine.remove(0, 3);       //A line starting with "#@!" is valid

            if( !currentLine.startsWith('#') && currentLine.contains('=') )  //Ignore comments and lines without '='
            {
                equalPosition = currentLine.indexOf('=');
                key = currentLine.left( equalPosition );
                value = currentLine.right( currentLine.size() - equalPosition - 1 );
                value.chop(1);      //Chop the last character ('/n')

                result = false;
                for(int i = COMMONARGS; i <= AUTOLEVELLERARGS && !result; i++)
                    result = args[i].setValue(key, value);

                if(result == false)
                    QMessageBox::information(this, "Error", "Invalid parameter in configuration file: key=" + key + " value=" + value);
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

    filename = QFileDialog::getSaveFileName(this, "Save configuration file", lastDir);
    if( !filename.isEmpty() )
        saveConfFile(filename);
}

void MainWindow::saveConfFile(QString filename)
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
            arguments = args[i].getAllArgs(true, true);
            confFile.write( QString("# " + names[i] + " options\n").toLatin1() );

            for( QStringList::const_iterator j = arguments.begin(); j != arguments.constEnd(); j++ )
                confFile.write( (*j + '\n').toLatin1() );

            confFile.write("\n");
        }

        confFile.close();
    }
    else
    {
        QMessageBox::information(this, "Error", "Can't save the file " + filename);
    }
}

void MainWindow::saveDefaultConfFile()
{
    QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if( appDataLocation.isEmpty() )
        QMessageBox::information(this, "Error", "Can't retrieve standard folder location");
    else
        if( QDir().mkpath(appDataLocation) )
            saveConfFile(appDataLocation + default_config_filename);
        else
            QMessageBox::information(this, "Error", "Can't create path " + appDataLocation);
}

void MainWindow::resetDefaultConfFile()
{
    QString appDataLocation;

    if( QMessageBox::question(this, "Reset default configuration",
                          "Are you sure you want to reset the default configuration?",
                          QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes )
    {
        appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if( appDataLocation.isEmpty() )
            QMessageBox::information(this, "Error", "Can't retrieve standard folder location");
        else
            if( !QFile::remove(appDataLocation + default_config_filename) )
                QMessageBox::information(this, "Error", "Can't delete default configuration file "
                                         + appDataLocation + default_config_filename);
    }
}

void MainWindow::loadDefaultConfFile()
{
    QString appDataLocation;

    appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if( appDataLocation.isEmpty() )
        QMessageBox::information(this, "Error", "Can't retrieve standard folder location");
    else
        loadConfFile(appDataLocation + default_config_filename);
}
