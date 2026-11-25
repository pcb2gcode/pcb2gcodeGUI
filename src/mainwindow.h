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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "pcb2gcode.h"

#include <QMainWindow>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

class OptionForm;
class OutputWindow;
class PreviewPane;
class QAction;
class QFrame;
class QLabel;
class QPushButton;
class QSettings;
class QSplitter;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &pcb2gcodePath = QString(), bool bundledOptions = false,
                        QWidget *parent = nullptr);

    bool openConfig(const QString &path);
    OptionForm *form() const { return m_form; }
    PreviewPane *preview() const { return m_preview; }
    void startPreview();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createWidgets();
    void createMenus();
    void loadSettings();
    void saveSettings();
    void setPcb2gcode(const QString &path);
    void loadStartupConfig();
    void updateTitle();
    void updateStatus();
    void formChanged();
    QStringList commandArguments() const;
    bool hasInputFiles() const;
    void schedulePreview();
    void previewFinished(int exitCode, QProcess::ExitStatus status);
    bool canCheckOptions() const;
    void startCheck();
    void checkFinished();
    void updateProblems();
    void generate();
    void generationFinished(bool success);
    void showCommandLine();
    void newConfig();
    void openConfigDialog();
    bool saveConfig();
    bool saveConfigAs();
    bool writeConfig(const QString &path);
    void saveDefaultConfig();
    void loadDefaultConfig();
    void resetDefaultConfig();
    void locatePcb2gcode();
    void aboutPcb2gcode();
    void aboutGui();
    QString defaultConfigPath() const;
    QString configHeader() const;
    void showNotes(const QString &what, const QStringList &notes);

    QSettings *m_settings;
    pcb2gcode::Probe m_probe;
    QString m_pcb2gcodePath;
    bool m_bundledOptions;

    OptionForm *m_form;
    PreviewPane *m_preview;
    QSplitter *m_splitter;
    QLabel *m_problems;
    QFrame *m_banner;
    QLabel *m_bannerText;
    QLabel *m_status;
    QPushButton *m_previewButton;
    QPushButton *m_generateButton;
    QAction *m_autoPreview;

    QProcess m_previewProcess;
    QTemporaryDir m_tempDir;
    QTimer m_previewTimer;
    bool m_previewAgain = false;
    QProcess m_checkProcess;
    QTimer m_checkTimer;
    QStringList m_checkArguments;
    bool m_checkAgain = false;
    OutputWindow *m_output = nullptr;
    QString m_runOutputDir;
    QString m_configPath;
};

#endif  // MAINWINDOW_H
