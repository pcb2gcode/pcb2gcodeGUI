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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define PCB2GCODEGUI_VERSION "0.1"

#include <QMainWindow>
#include <QProcess>
#include <QStringList>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QPlainTextEdit>
#include <QHash>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QGraphicsScene>
#include <QMovie>
#include "outputwindow.h"
#include "argaction.h"
#include "graphics_view_zoom.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void vectorialEnable(bool enable);
    void voronoiEnable(bool disable);
    void bridgesAvailable();
    void changeMetricInputUnits(bool metric);
    void startPcb2gcode();
    void pcb2gcodeStopped();
    void pcb2gcodeError(QProcess::ProcessError error);
    void printOutput();
    void killCloseButtonClicked();
    void changeKillCloseButtonText(QProcess::ProcessState state);
    void enableStartButton();

    void generateImages();
    void imagesGenerated(int exitCode, QProcess::ExitStatus exitStatus);
    void imageSelected(int index);
    void addImageFile(const QDir& dir, const QString& item, const QString& filename);

    void getFrontFile();
    void getBackFile();
    void getOutlineFile();
    void getDrillFile();
    void getPreambleFile();
    void getPreambletextFile();
    void getPostambleFile();
    void getOutputDirectory();

    void menu_showCommandLineArguments();
    void menu_aboutpcb2gcode();
    void menu_aboutpcb2gcodeGUI();
    void menu_manual();

    void askAndSaveConfFile();
    void askAndLoadConfFile();
    void saveDefaultConfFile();
    void resetDefaultConfFile();
    void loadDefaultConfFile();

    void updateAlCustomEnableState(QString text);

private:
    enum ArgsType { FILEARGS = 0, COMMONARGS = 1, MILLARGS = 2, DRILLARGS = 3, OUTLINEARGS = 4, AUTOLEVELLERARGS = 5 };
    static const QString names[];

    Ui::MainWindow *ui;
    QSettings *settings;

    QButtonGroup inputUnits;
    QButtonGroup outputUnits;
    QButtonGroup mirrorType;

    const QString pcb2gcodeVersion;
    QString lastDir;
    QProcess pcb2gcodeProcess;
    bool pcb2gcodeKilled;
    bool changeMetricImperialValues;

    Graphics_view_zoom* gview_zoom;
    QProcess pcb2gcodeImageProcess;
    QGraphicsScene scene;
    QMovie loadingIcon;
    const QString imagesFolder;
    QStringList imagesFilename;
    QString currentImagesFolder;
    bool vectorial;
    bool fillOutline;
    bool restarted;

    argAction args[6];

    outputWindow *pcb2gcodeOutputWindow;
    QPlainTextEdit *outputTextEdit;
    QPushButton *killClosePushButton;

    void closeEvent(QCloseEvent *);
    void initUi();
    void checkPcb2gcodeVersion();
    QStringList getCmdLineArguments();
    bool getFilename(QLineEdit *saveTo, const QString name, QString filter);
    void adjustMetricImperial(QSpinBox *spinBox, const double cfactor, const QString suffix);
    void adjustMetricImperial(QDoubleSpinBox *doubleSpinBox, const double cfactor, const QString suffix);
    void saveConfFile(const QString filename);
    bool loadConfFile(const QString filename);
    QString getPcb2gcodeVersion();
    void clearImages();

    void showImage(QString image);
};

#endif // MAINWINDOW_H
