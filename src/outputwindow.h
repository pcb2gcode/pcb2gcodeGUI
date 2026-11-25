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

#ifndef OUTPUTWINDOW_H
#define OUTPUTWINDOW_H

#include <QDialog>
#include <QProcess>

class QLabel;
class QPlainTextEdit;
class QPushButton;

// Runs pcb2gcode and shows its output.
class OutputWindow : public QDialog
{
    Q_OBJECT
public:
    explicit OutputWindow(QWidget *parent = nullptr);

    void start(const QString &program, const QStringList &arguments, const QString &workingDirectory);
    bool isRunning() const { return m_process.state() != QProcess::NotRunning; }
    QString output() const;

signals:
    void finished(bool success);

protected:
    void reject() override;

private:
    void append(const QString &text, const QColor &color = QColor());
    void processFinished(int exitCode, QProcess::ExitStatus status);

    QPlainTextEdit *m_text;
    QLabel *m_status;
    QPushButton *m_button;
    QProcess m_process;
    bool m_killed = false;
};

#endif  // OUTPUTWINDOW_H
