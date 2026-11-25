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

#include "outputwindow.h"

#include "pcb2gcode.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>

OutputWindow::OutputWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("pcb2gcode output"));
    resize(760, 460);

    m_text = new QPlainTextEdit;
    m_text->setReadOnly(true);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_status = new QLabel;
    m_button = new QPushButton(tr("Stop pcb2gcode"));
    connect(m_button, &QPushButton::clicked, this, [this] {
        if (isRunning()) {
            m_killed = true;
            m_process.kill();
        } else {
            accept();
        }
    });

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_status, 1);
    buttons->addWidget(m_button);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_text, 1);
    layout->addLayout(buttons);

    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this,
            [this] { append(QString::fromLocal8Bit(m_process.readAllStandardOutput())); });
    connect(&m_process, &QProcess::finished, this, &OutputWindow::processFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            append(tr("\npcb2gcode failed to start: %1\n").arg(m_process.errorString()), Qt::red);
            m_status->setText(tr("Failed to start"));
            m_button->setText(tr("Close"));
            emit finished(false);
        }
    });
}

void OutputWindow::start(const QString &program, const QStringList &arguments, const QString &workingDirectory)
{
    m_killed = false;
    m_text->clear();
    append("$ " + pcb2gcode::shellCommand(program, arguments) + "\n\n", QColor(0x30, 0x60, 0xa0));
    m_status->setText(tr("Running…"));
    m_button->setText(tr("Stop pcb2gcode"));
    m_process.setWorkingDirectory(workingDirectory);
    m_process.start(program, arguments, QIODevice::ReadOnly);
}

QString OutputWindow::output() const
{
    return m_text->toPlainText();
}

void OutputWindow::append(const QString &text, const QColor &color)
{
    QTextCharFormat format;
    if (color.isValid())
        format.setForeground(color);
    QTextCursor cursor = m_text->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, format);
    m_text->verticalScrollBar()->setValue(m_text->verticalScrollBar()->maximum());
}

void OutputWindow::processFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool ok = status == QProcess::NormalExit && exitCode == 0;
    if (m_killed)
        m_status->setText(tr("Stopped"));
    else if (status != QProcess::NormalExit)
        m_status->setText(tr("pcb2gcode crashed"));
    else if (ok)
        m_status->setText(tr("Done"));
    else
        m_status->setText(tr("pcb2gcode failed with exit code %1").arg(exitCode));
    m_status->setStyleSheet(ok ? QString() : QStringLiteral("color: #b02020;"));
    m_button->setText(tr("Close"));
    emit finished(ok);
}

void OutputWindow::reject()
{
    if (isRunning()) {
        m_killed = true;
        m_process.kill();
        m_process.waitForFinished(2000);
    }
    QDialog::reject();
}
