/*
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

#include "pcb2gcode.h"

#include "condition.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <algorithm>

namespace pcb2gcode {

const char *const kDumpOptionsFlag = "--dump-options";
const char *const kCheckOptionsFlag = "--check-options";

std::optional<CheckResult> parseCheckOutput(const QByteArray &output)
{
    const QJsonDocument doc = QJsonDocument::fromJson(output);
    const QJsonObject root = doc.object();
    if (!doc.isObject() || root.value("format_version").toInt() != 1)
        return std::nullopt;
    auto read = [](const QJsonValue &list) {
        QList<CheckIssue> issues;
        for (const QJsonValue &v : list.toArray()) {
            const QJsonObject o = v.toObject();
            CheckIssue issue;
            for (const QJsonValue &name : o.value("options").toArray())
                issue.options.append(name.toString());
            issue.rule = o.value("rule").toString();
            issue.message = o.value("message").toString();
            issue.code = o.value("code").toInt();
            issues.append(issue);
        }
        return issues;
    };
    CheckResult result;
    result.errors = read(root.value("errors"));
    result.warnings = read(root.value("warnings"));
    return result;
}

std::optional<CheckResult> checkOptions(const QString &program, const QStringList &arguments, int timeoutMs)
{
    QByteArray out;
    if (!run(program, QStringList{QString::fromLatin1(kCheckOptionsFlag)} + arguments, timeoutMs, &out))
        return std::nullopt;
    return parseCheckOutput(out);
}

QString executableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("pcb2gcode.exe");
#else
    return QStringLiteral("pcb2gcode");
#endif
}

QString locate(const QString &configuredPath)
{
    if (!configuredPath.isEmpty()) {
        const QFileInfo fi(configuredPath);
        if (fi.isFile() && fi.isExecutable())
            return fi.absoluteFilePath();
        const QString inPath = QStandardPaths::findExecutable(configuredPath);
        if (!inPath.isEmpty())
            return inPath;
    }
    const QFileInfo beside(QDir(QCoreApplication::applicationDirPath()).filePath(executableName()));
    if (beside.isFile() && beside.isExecutable())
        return beside.absoluteFilePath();
    const QString onPath = QStandardPaths::findExecutable(executableName());
    if (!onPath.isEmpty())
        return onPath;
    return QStandardPaths::findExecutable(executableName(),
                                          {QStringLiteral("/opt/homebrew/bin"),
                                           QStringLiteral("/usr/local/bin"),
                                           QStringLiteral("/opt/local/bin"),
                                           QDir::homePath() + QStringLiteral("/.local/bin")});
}

bool run(const QString &program, const QStringList &arguments, int timeoutMs, QByteArray *output,
         int *exitCode, QString *error)
{
    QTemporaryDir scratch;
    QProcess process;
    // A stray millproject in the working directory must not affect the probe.
    if (scratch.isValid())
        process.setWorkingDirectory(scratch.path());
    process.start(program, arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted(timeoutMs)) {
        if (error)
            *error = process.errorString();
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        if (error)
            *error = QObject::tr("%1 didn't finish in time").arg(QFileInfo(program).fileName());
        return false;
    }
    if (output)
        *output = process.readAllStandardOutput();
    if (exitCode)
        *exitCode = process.exitCode();
    if (error)
        *error = QString::fromLocal8Bit(process.readAllStandardError());
    return process.exitStatus() == QProcess::NormalExit;
}

int compareVersions(const QString &a, const QString &b)
{
    const QStringList pa = a.split('.'), pb = b.split('.');
    for (int i = 0; i < std::max(pa.size(), pb.size()); i++) {
        const int x = i < pa.size() ? pa.at(i).toInt() : 0;
        const int y = i < pb.size() ? pb.at(i).toInt() : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

Probe probe(const QString &program, bool forceBundled)
{
    Probe p;
    p.program = program;
    OptionSchema bundled = OptionSchema::bundled();

    if (!program.isEmpty()) {
        QByteArray out;
        int code = -1;
        if (run(program, {QStringLiteral("--version")}, 5000, &out, &code) && code == 0) {
            p.versionText = QString::fromLocal8Bit(out).trimmed();
            const auto m = QRegularExpression(QStringLiteral(R"(\d+\.\d+\.\d+)")).match(p.versionText);
            p.version = m.hasMatch() ? m.captured() : p.versionText.section('\n', 0, 0);
            const auto g = QRegularExpression(QStringLiteral(R"(^Git commit:\s*(\S+))"),
                                              QRegularExpression::MultilineOption)
                               .match(p.versionText);
            if (g.hasMatch())
                p.gitCommit = g.captured(1);
        } else {
            p.program.clear();
        }
    }

    if (p.program.isEmpty()) {
        p.schema = bundled;
        p.schemaNote = QObject::tr("pcb2gcode not found; showing the options of pcb2gcode %1")
                           .arg(bundled.pcb2gcodeVersion);
        return p;
    }

    if (!forceBundled) {
        QByteArray out;
        int code = -1;
        if (run(p.program, {QString::fromLatin1(kDumpOptionsFlag), QStringLiteral("--noconfigfile")}, 10000,
                &out, &code) &&
            code == 0) {
            OptionSchema dumped = OptionSchema::fromJson(out);
            if (dumped.isValid()) {
                dumped.source = OptionSchema::Source::Dump;
                if (dumped.pcb2gcodeVersion.isEmpty())
                    dumped.pcb2gcodeVersion = p.version;
                if (dumped.rules.isEmpty()) {
                    for (const OptionRule &rule : bundled.rules) {
                        QStringList names = rule.options;
                        names += condition::options(rule.unless) + condition::options(rule.condition);
                        bool applies = true;
                        for (const QString &name : names)
                            applies = applies && dumped.find(name);
                        if (applies)
                            dumped.rules.append(rule);
                    }
                }
                p.schema = dumped;
                p.schemaNote = QObject::tr("Options from pcb2gcode %1").arg(p.version);
                return p;
            }
        }
    }

    p.schema = bundled;
    QByteArray help;
    int code = -1;
    if (run(p.program, {QStringLiteral("--help")}, 5000, &help, &code) && code == 0) {
        p.schema.restrictTo(OptionSchema::parseHelp(QString::fromLocal8Bit(help)));
        p.schemaNote = QObject::tr("Built-in option list for pcb2gcode %1, checked against pcb2gcode %2 --help")
                           .arg(bundled.pcb2gcodeVersion, p.version);
    } else {
        p.schemaNote = QObject::tr("Built-in option list for pcb2gcode %1").arg(bundled.pcb2gcodeVersion);
    }
    p.olderThanSchema = compareVersions(p.version, bundled.pcb2gcodeVersion) < 0;
    return p;
}

QString shellCommand(const QString &program, const QStringList &arguments)
{
    QStringList parts{program};
    parts += arguments;
    for (QString &a : parts) {
#ifdef Q_OS_WIN
        if (a.isEmpty() || a.contains(QRegularExpression(QStringLiteral(R"([\s"&|<>^])"))))
            a = '"' + QString(a).replace('"', QStringLiteral("\"\"")) + '"';
#else
        static const QRegularExpression safe(QStringLiteral(R"(^[A-Za-z0-9_@%+=:,./-]+$)"));
        if (!safe.match(a).hasMatch())
            a = '\'' + QString(a).replace('\'', QStringLiteral("'\\''")) + '\'';
#endif
    }
    return parts.join(' ');
}

}  // namespace pcb2gcode
