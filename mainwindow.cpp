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
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDateTime>
#include "settings.h"

const QString MainWindow::names[] = { "File", "Common", "Mill", "Drill", "Outline", "Autoleveller" };

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    inputGroup(this),
    outputGroup(this),
    pcb2gcodeProcess(this),
    pcb2gcodeKilled(false),
    changeMetricImperialValues(true)
{
    QString appDataLocation;

    ui->setupUi(this);
    this->setFixedSize(this->width(), this->height());

    inputGroup.addButton(ui->inputMetricRadioButton, 0);
    inputGroup.addButton(ui->inputImperialRadioButton, 1);
    outputGroup.addButton(ui->outputMetricRadioButton, 0);
    outputGroup.addButton(ui->outputImperialRadioButton, 1);
    inputGroup.button(0)->setChecked(true);
    outputGroup.button(0)->setChecked(true);

    input = new QWidgetPair<QRadioButton, QRadioButton>
            (ui->inputMetricRadioButton, ui->inputImperialRadioButton);
    output = new QWidgetPair<QRadioButton, QRadioButton>
            (ui->outputMetricRadioButton, ui->outputImperialRadioButton);

    args[ FILEARGS ].insert("front", ui->frontLineEdit);
    args[ FILEARGS ].insert("back", ui->backLineEdit);
    args[ FILEARGS ].insert("outline", ui->outlineLineEdit);
    args[ FILEARGS ].insert("drill", ui->drillLineEdit);
    args[ FILEARGS ].insert("preamble", ui->preambleLineEdit);
    args[ FILEARGS ].insert("preamble-text", ui->preambletextLineEdit);
    args[ FILEARGS ].insert("postamble", ui->postambleLineEdit);
    args[ FILEARGS ].insert("output-dir", ui->outputDirLineEdit);

    args[ COMMONARGS ].insert("metric", input);
    args[ COMMONARGS ].insert("metricoutput", output);
    args[ COMMONARGS ].insert("zsafe", ui->zsafeDoubleSpinBox);
    args[ COMMONARGS ].insert("zchange", ui->zchangeDoubleSpinBox);
    args[ COMMONARGS ].insert("g64", ui->g64DoubleSpinBox);
    args[ COMMONARGS ].insert("optimise", ui->optimiseCheckBox);
    args[ COMMONARGS ].insert("zero-start", ui->zerostartCheckBox);
    args[ COMMONARGS ].insert("mirror-absolute", ui->mirrorabsoluteCheckBox);
    args[ COMMONARGS ].insert("svg", ui->svgLineEdit);
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
    args[ AUTOLEVELLERARGS ].insert("al-probefeed", ui->alprobefeedSpinBox);
    args[ AUTOLEVELLERARGS ].insert("al-probeon", ui->alprobeonLineEdit);
    args[ AUTOLEVELLERARGS ].insert("al-probeoff", ui->alprobeoffLineEdit);

    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionAbout_pcb2gcode, SIGNAL(triggered()), this, SLOT(menu_aboutpcb2gcode()));
    connect(ui->actionAbout_pcb2gcodeGUI, SIGNAL(triggered()), this, SLOT(menu_aboutpcb2gcodeGUI()));
    connect(ui->actionManual, SIGNAL(triggered()), this, SLOT(menu_manual()));

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

    connect(ui->g64CheckBox, SIGNAL(toggled(bool)), ui->g64DoubleSpinBox, SLOT(setEnabled(bool)));
    connect(ui->filloutlineCheckBox, SIGNAL(toggled(bool)), ui->outlinewidthDoubleSpinBox, SLOT(setEnabled(bool)));
    connect(ui->svgCheckBox, SIGNAL(toggled(bool)), ui->svgLineEdit, SLOT(setEnabled(bool)));
    connect(ui->startPushButton, SIGNAL(clicked()), this, SLOT(startPcb2gcode()));
    connect(ui->inputMetricRadioButton, SIGNAL(toggled(bool)), this, SLOT(changeMetricInputUnits(bool)));

    connect(&pcb2gcodeProcess, SIGNAL(finished(int)), this, SLOT(pcb2gcodeStopped()));
    connect(&pcb2gcodeProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(pcb2gcodeError(QProcess::ProcessError)));
    connect(&pcb2gcodeProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(printOutput()));
    connect(&pcb2gcodeProcess, SIGNAL(readyReadStandardError()), this, SLOT(printOutput()));
    connect(&pcb2gcodeProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(changeKillCloseButtonText(QProcess::ProcessState)));

    lastDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if( lastDir.isEmpty() )
        QMessageBox::information(this, tr("Error"), tr("Can't retrieve home location"));

    appDataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if( appDataLocation.isEmpty() )
        QMessageBox::information(this, tr("Error"), tr("Can't retrieve standard folder location"));
    else
        loadConfFile(appDataLocation + default_config_filename);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::getFrontFile()
{
    getFilename(ui->frontLineEdit, tr("front file"));
}

void MainWindow::getBackFile()
{
    getFilename(ui->backLineEdit, tr("back file"));
}

void MainWindow::getOutlineFile()
{
    getFilename(ui->outlineLineEdit, tr("outline file"));
}

void MainWindow::getDrillFile()
{
    getFilename(ui->drillLineEdit, tr("drill file"));
}

void MainWindow::getPreambleFile()
{
    getFilename(ui->preambleLineEdit, tr("preamble file"));
}

void MainWindow::getPreambletextFile()
{
    getFilename(ui->preambletextLineEdit, tr("preamble text file"));
}

void MainWindow::getPostambleFile()
{
    getFilename(ui->postambleLineEdit, tr("postamble file"));
}

void MainWindow::getFilename(QLineEdit *saveTo, const QString name)
{
    QString filename;

    filename = QFileDialog::getOpenFileName(this, tr("Select the ") + name, lastDir );
    if( !filename.isEmpty() )
    {
        lastDir = QFileInfo(filename).path();
        saveTo->setText( filename );   
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
                                                  ui->alyDoubleSpinBox, ui->g64DoubleSpinBox };

    QSpinBox *spinBoxes[] = { ui->millfeedSpinBox, ui->drillfeedSpinBox, ui->cutfeedSpinBox, ui->alprobefeedSpinBox };

    const unsigned int doubleSpinBoxesLen =  sizeof(doubleSpinBoxes) / sizeof(doubleSpinBoxes[0]);
    const unsigned int spinBoxesLen =  sizeof(spinBoxes) / sizeof(spinBoxes[0]);
    const double cfactor = metric ? 1/2.54 : 2.54;
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
    int pos;

    arguments += args[ FILEARGS ].getAllArgs(false);
    arguments += args[ COMMONARGS ].getAllArgs(false);

    if( !ui->frontLineEdit->text().isEmpty() || !ui->backLineEdit->text().isEmpty() )
        arguments += args[ MILLARGS ].getAllArgs(false);

    if( !ui->drillLineEdit->text().isEmpty() )
        arguments += args[ DRILLARGS ].getAllArgs(false);

    if( !ui->outlineLineEdit->text().isEmpty() )
        arguments += args[ OUTLINEARGS ].getAllArgs(false);

    if ( (ui->alfrontCheckBox->isChecked() || ui->albackCheckBox->isChecked()) &&
         (!ui->frontLineEdit->text().isEmpty() || !ui->backLineEdit->text().isEmpty()) )
        arguments += args[ AUTOLEVELLERARGS ].getAllArgs(false);

    i = 0;

    while( i < arguments.size() )
    {
        if( arguments[i].contains("al-probe-") )
        {
            pos = arguments[i].indexOf('=');

            if( pos > 0 )
            {
                arguments[i].prepend("--");
                arguments[i].insert( pos + 1, '\'');
                arguments[i].append('\'');
                i++;
            }
            else
                arguments.removeAt(i);
        }
        else if( !arguments[i].contains("=false") )   //Remove lines with a false boolean option ("--metric=false")
        {
            arguments[i].remove("=true");   //Remove =true ("--metric=true" --> "--metric")
            arguments[i].prepend("--");     //Add the "--"
            i++;
        }
        else
            arguments.removeAt(i);
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

        outputTextEdit = pcb2gcodeOutputWindow->getPlainTextEdit();
        outputTextEdit->appendPlainText(QString(tr("Starting ")) + PCB2GCODE_EXECUTABLE + ' ' + arguments.join(' ')) ;

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

void MainWindow::menu_aboutpcb2gcode()
{
    QMessageBox msgBox(this);
    QString version;

    msgBox.setWindowTitle(tr("About pcb2gcode"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText( QString(tr(about_pcb2gcode_str)).arg(tr("(retrieving version...)")) );
    msgBox.show();

    version = getPcb2gcodeVersion();

    if( version.isEmpty() )
        msgBox.setText( QString(tr(about_pcb2gcode_str)).arg(tr("(<font color=\"Red\">can't run" PCB2GCODE_EXECUTABLE "</font>)")) );
    else
        msgBox.setText( QString(tr(about_pcb2gcode_str)).arg(QString(version)) );

    msgBox.exec();
}

QString MainWindow::getPcb2gcodeVersion()
{
    QProcess pcb2gcodeVersionProcess(this);
    QByteArray version;

    pcb2gcodeVersionProcess.start(PCB2GCODE_EXECUTABLE, QStringList("--version"), QProcess::ReadOnly);
    pcb2gcodeVersionProcess.waitForReadyRead(2000);
    version = pcb2gcodeVersionProcess.readAllStandardOutput();

    return QString(version);
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

                result = false;
                for(int i = COMMONARGS; i <= AUTOLEVELLERARGS && !result; i++)
                {
                    result = args[i].setValue(key, value);
                    if(result)
                        args[i].setEnabled(key, enabledOption);
                }

                if(result == false)
                    QMessageBox::information(this, tr("Error"), tr("Invalid parameter in configuration file: key=") + key + tr(" value=") + value);
            }
        }

        ui->g64CheckBox->setChecked(ui->g64DoubleSpinBox->isEnabled());   //Sync checkBox checked state with doubleSpinBox enabled state
        ui->svgCheckBox->setChecked(ui->svgLineEdit->isEnabled());      //Sync checkBox checked state with lineEdit enabled state

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
            arguments = args[i].getAllArgs(true);
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
