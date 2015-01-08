#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#define PCB2GCODEGUI_VERSION "0.1"

#include <QMainWindow>
#include <QProcess>
#include <QStringList>
#include <QLineEdit>
#include <QButtonGroup>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QHash>
#include <QStandardPaths>
#include <QDir>
#include "outputwindow.h"
#include "argaction.h"

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
    void changeMetricInputUnits(bool metric);
    void startPcb2gcode();
    void pcb2gcodeStopped();
    void pcb2gcodeError(QProcess::ProcessError error);
    void printOutput();
    void killCloseButtonClicked();
    void changeKillCloseButtonText(QProcess::ProcessState state);
    void enableStartButton();

    void getFrontFile();
    void getBackFile();
    void getOutlineFile();
    void getDrillFile();
    void getPreambleFile();
    void getPreambletextFile();
    void getPostambleFile();
    void getOutputDirectory();

    void menu_aboutpcb2gcode();
    void menu_aboutpcb2gcodeGUI();
    void menu_manual();

    void askAndSaveConfFile();
    void askAndLoadConfFile();
    void saveDefaultConfFile();
    void resetDefaultConfFile();

private:
    enum SystemOfMeasurement { METRIC, IMPERIAL };
    enum ArgsType { FILEARGS = 0, COMMONARGS = 1, MILLARGS = 2, DRILLARGS = 3, OUTLINEARGS = 4, AUTOLEVELLERARGS = 5 };
    static const QString names[];

    Ui::MainWindow *ui;

    QString lastDir;
    QButtonGroup inputSystemOfMeasurement;
    QButtonGroup outputSystemOfMeasurement;
    QProcess pcb2gcodeProcess;
    bool pcb2gcodeKilled;
    bool changeMetricImperialValues;

    argAction args[6];

    outputWindow *pcb2gcodeOutputWindow;
    QPlainTextEdit *outputTextEdit;
    QPushButton *killClosePushButton;

    void enableImperialMetricAdjustment(bool enable);
    QStringList getCmdLineArguments();
    void getFilename(QLineEdit *saveTo, QString name);
    void adjustMetricImperial(QSpinBox *spinBox, const double cfactor, const QString suffix);
    void adjustMetricImperial(QDoubleSpinBox *doubleSpinBox, const double cfactor, const QString suffix);
    void saveConfFile(QString filename);
    bool loadConfFile(QString filename);
    void loadDefaultConfFile();
};

#endif // MAINWINDOW_H
