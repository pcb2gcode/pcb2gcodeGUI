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

#include "previews.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Previews", text);
}

int layerRank(const QString &layer)
{
    static const QStringList order = {"front", "back", "outline", "drill", "milldrill"};
    const int i = order.indexOf(layer);
    return i < 0 ? order.size() : i;
}

int stageRank(const PreviewImage &image)
{
    // Final toolpaths first: they are what the G-code does.
    if (image.stage == "processed")
        return image.final || !image.hasFinalTwin ? 0 : 3;
    if (image.stage == "traced")
        return image.final || !image.hasFinalTwin ? 1 : 4;
    if (image.stage == "contentions")
        return 2;
    if (image.stage == "masked")
        return 5;
    if (image.stage == "original")
        return 6;
    return 7;
}

QString layerName(const QString &layer)
{
    if (layer == "front")
        return tr("Front");
    if (layer == "back")
        return tr("Back");
    if (layer == "outline")
        return tr("Outline");
    if (layer == "drill")
        return tr("Drill");
    if (layer == "milldrill")
        return tr("Milldrill");
    return layer;
}

void finish(QList<PreviewImage> &images)
{
    QHash<QString, QSet<int>> tools;
    QSet<QString> finals;
    for (const PreviewImage &i : images) {
        if (i.toolIndex >= 0)
            tools[i.layer].insert(i.toolIndex);
        if (i.final)
            finals.insert(i.layer + '/' + i.stage + '/' + QString::number(i.toolIndex));
    }
    for (PreviewImage &i : images) {
        i.layerToolCount = std::max(1, int(tools.value(i.layer).size()));
        i.hasFinalTwin = !i.final && finals.contains(i.layer + '/' + i.stage + '/' + QString::number(i.toolIndex));
    }
    sortPreviews(images);
}

}  // namespace

QString PreviewImage::label() const
{
    QString what;
    if (stage == "processed")
        what = layer == "outline" ? tr("cutting path and board") : tr("toolpaths and copper");
    else if (stage == "traced")
        what = layer == "outline" ? tr("cutting path") : tr("toolpaths");
    else if (stage == "contentions")
        what = tr("clearance problems");
    else if (stage == "masked")
        what = tr("copper inside the outline");
    else if (stage == "original")
        what = layer == "drill" || layer == "milldrill" ? tr("holes") : tr("input");
    else
        what = stage;

    QString text = layerName(layer) + ": " + what;
    if (!linesWidth.isEmpty())
        text += tr(", Gerber lines %1 in").arg(linesWidth);
    else if (toolIndex >= 0 && layerToolCount > 1) {
        text += tr(", tool %1").arg(toolIndex + 1);
        if (toolDiameterMm > 0)
            text += QStringLiteral(" (%1 mm)").arg(QString::number(toolDiameterMm, 'g', 4));
    }
    if (hasFinalTwin)
        text += tr(" (before optimisation)");
    return text;
}

void sortPreviews(QList<PreviewImage> &images)
{
    std::stable_sort(images.begin(), images.end(), [](const PreviewImage &a, const PreviewImage &b) {
        const int sa = stageRank(a), sb = stageRank(b);
        if (sa != sb)
            return sa < sb;
        const int la = layerRank(a.layer), lb = layerRank(b.layer);
        if (la != lb)
            return la < lb;
        if (a.toolIndex != b.toolIndex)
            return a.toolIndex < b.toolIndex;
        return a.path < b.path;
    });
}

QList<PreviewImage> previewsFromManifest(const QString &manifestPath, QString *error)
{
    QList<PreviewImage> images;
    QFile f(manifestPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return images;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject root = doc.object();
    if (root.value("format_version").toInt() != 1) {
        if (error)
            *error = tr("unsupported manifest format");
        return images;
    }
    const QDir base = QFileInfo(manifestPath).absoluteDir();
    for (const QJsonValue &v : root.value("files").toArray()) {
        const QJsonObject o = v.toObject();
        if (o.value("type").toString() != "svg")
            continue;
        PreviewImage image;
        image.path = QDir::cleanPath(base.filePath(o.value("path").toString()));
        image.layer = o.value("layer").toString();
        image.stage = o.value("stage").toString();
        image.final = o.value("final").toBool();
        image.toolIndex = o.value("tool_index").toInt(-1);
        image.toolDiameterMm = o.value("tool_diameter_mm").toDouble(-1);
        static const QRegularExpression lines(QStringLiteral(R"(_lines_([0-9.e+-]+)\.svg$)"));
        const auto m = lines.match(image.path);
        if (m.hasMatch())
            image.linesWidth = m.captured(1);
        images.append(image);
    }
    finish(images);
    return images;
}

QList<PreviewImage> previewsFromFileNames(const QString &directory)
{
    static const QRegularExpression inputRe(
        QStringLiteral(R"(^outp\d+_(original|masked)_(front|back|outline)\.svg$)"));
    static const QRegularExpression holesRe(QStringLiteral(R"(^original_(drill|milldrill)\.svg$)"));
    static const QRegularExpression pathsRe(QStringLiteral(
        R"(^(processed|traced|contentions)_(front|back|outline)(_final)?(?:_lines_([0-9.e+-]+)|_(\d+))?\.svg$)"));

    QList<PreviewImage> images;
    const QDir dir(directory);
    for (const QString &name : dir.entryList({QStringLiteral("*.svg")}, QDir::Files, QDir::Name)) {
        PreviewImage image;
        image.path = dir.filePath(name);
        if (const auto m = inputRe.match(name); m.hasMatch()) {
            image.stage = m.captured(1);
            image.layer = m.captured(2);
        } else if (const auto h = holesRe.match(name); h.hasMatch()) {
            image.stage = QStringLiteral("original");
            image.layer = h.captured(1);
        } else if (const auto p = pathsRe.match(name); p.hasMatch()) {
            image.stage = p.captured(1);
            image.layer = p.captured(2);
            image.final = !p.captured(3).isEmpty();
            image.linesWidth = p.captured(4);
            image.toolIndex = p.captured(5).isEmpty() ? 0 : p.captured(5).toInt();
        } else {
            continue;
        }
        images.append(image);
    }
    finish(images);
    return images;
}

QList<PreviewImage> collectPreviews(const QString &directory, const QString &manifestPath, bool *fromManifest)
{
    if (fromManifest)
        *fromManifest = false;
    if (!manifestPath.isEmpty() && QFileInfo::exists(manifestPath)) {
        const QList<PreviewImage> listed = previewsFromManifest(manifestPath);
        if (!listed.isEmpty()) {
            if (fromManifest)
                *fromManifest = true;
            return listed;
        }
    }
    return previewsFromFileNames(directory);
}
