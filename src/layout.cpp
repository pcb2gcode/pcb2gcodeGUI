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

#include "layout.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QStringList strings(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &s : v.toArray())
        out.append(s.toString());
    return out;
}

}  // namespace

Layout Layout::fromJson(const QByteArray &json, QString *error)
{
    Layout layout;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        if (error)
            *error = parseError.errorString();
        return layout;
    }
    const QJsonObject root = doc.object();
    layout.hidden = strings(root.value("hidden"));

    const QJsonObject groupTabs = root.value("group_tabs").toObject();
    for (auto it = groupTabs.begin(); it != groupTabs.end(); ++it)
        layout.groupTabs.insert(it.key(), it.value().toString());

    for (const QJsonValue &t : root.value("tabs").toArray()) {
        const QJsonObject o = t.toObject();
        LayoutTab tab;
        tab.id = o.value("id").toString();
        tab.title = o.value("title").toString();
        tab.wiki = o.value("wiki").toString();
        for (const QJsonValue &s : o.value("sections").toArray()) {
            const QJsonObject so = s.toObject();
            tab.sections.append({so.value("title").toString(), strings(so.value("options"))});
        }
        layout.tabs.append(tab);
    }

    const QJsonObject options = root.value("options").toObject();
    for (auto it = options.begin(); it != options.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        OptionHint hint;
        hint.label = o.value("label").toString();
        hint.preset = o.value("preset").toString();
        hint.enabledIf = o.value("enabled_if").toString();
        hint.placeholder = o.value("placeholder").toString();
        hint.minimum = o.value("min").toString();
        hint.perSecond = o.value("per_second").toBool();
        hint.fine = o.value("fine").toBool();
        const QJsonObject labels = o.value("choice_labels").toObject();
        for (auto l = labels.begin(); l != labels.end(); ++l)
            hint.choiceLabels.insert(l.key(), l.value().toString());
        layout.hints.insert(it.key(), hint);
    }
    return layout;
}

Layout Layout::bundled()
{
    QFile file(QStringLiteral(":/gui-layout.json"));
    if (!file.open(QIODevice::ReadOnly))
        return Layout();
    return fromJson(file.readAll());
}

QStringList Layout::placedOptions() const
{
    QStringList names;
    for (const LayoutTab &tab : tabs)
        for (const LayoutSection &section : tab.sections)
            names += section.options;
    return names;
}
