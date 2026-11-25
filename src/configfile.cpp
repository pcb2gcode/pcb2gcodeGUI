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

#include "configfile.h"

#include "optionschema.h"
#include "units.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

ConfigFile ConfigFile::parse(const QString &text)
{
    ConfigFile file;
    for (QString line : text.split('\n')) {
        line = line.trimmed();
        bool disabled = false;
        if (line.startsWith(QStringLiteral("#@#"))) {
            disabled = true;
            line = line.mid(3);
        }
        // Like Boost's config file parser, everything after a '#' is a comment.
        const int hash = line.indexOf('#');
        if (hash >= 0)
            line.truncate(hash);
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('['))
            continue;
        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;
        ConfigEntry entry;
        entry.key = line.left(eq).trimmed();
        entry.value = line.mid(eq + 1).trimmed();
        entry.disabled = disabled;
        file.entries.append(entry);
    }
    return file;
}

ConfigFile ConfigFile::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = f.errorString();
        return ConfigFile();
    }
    return parse(QString::fromUtf8(f.readAll()));
}

QString ConfigFile::serialize(const QString &header) const
{
    QString out;
    if (!header.isEmpty()) {
        for (const QString &line : header.split('\n'))
            out += "# " + line + "\n";
    }
    QString section;
    bool first = true;
    for (const ConfigEntry &e : entries) {
        if (first || e.section != section) {
            section = e.section;
            if (!out.isEmpty())
                out += '\n';
            if (!section.isEmpty())
                out += "# " + section + "\n";
            first = false;
        }
        out += (e.disabled ? "#@#" : "") + e.key + '=' + e.value + '\n';
    }
    return out;
}

bool ConfigFile::save(const QString &path, const QString &header, QString *error) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    f.write(serialize(header).toUtf8());
    if (!f.commit()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

QStringList ConfigFile::values(const QString &key, bool disabled) const
{
    QStringList out;
    for (const ConfigEntry &e : entries)
        if (e.key == key && e.disabled == disabled)
            out.append(e.value);
    return out;
}

bool ConfigFile::contains(const QString &key) const
{
    for (const ConfigEntry &e : entries)
        if (e.key == key)
            return true;
    return false;
}

namespace {

std::optional<bool> boolValue(const QString &value)
{
    const QString v = value.trimmed().toLower();
    if (v.isEmpty() || v == "1" || v == "true" || v == "yes" || v == "on")
        return true;
    if (v == "0" || v == "false" || v == "no" || v == "off")
        return false;
    return std::nullopt;
}

// Doubles a length while keeping its unit: "0.1mm" -> "0.2mm", "0.004" -> "0.008".
QString doubledLength(const QString &value)
{
    const auto q = units::parse(value, units::Dimension::Length, true);
    if (!q)
        return QString();
    return units::formatNumber(q->value * 2, 8) + QString(q->unit).remove(' ');
}

}  // namespace

QStringList migrateLegacyEntries(QList<ConfigEntry> &entries, const OptionSchema &schema)
{
    QStringList notes;
    auto has = [&entries](const QString &key) {
        for (const ConfigEntry &e : entries)
            if (e.key == key)
                return true;
        return false;
    };

    QList<ConfigEntry> out;
    for (const ConfigEntry &e : entries) {
        const OptionSpec *spec = schema.find(e.key);
        if (spec && spec->deprecated) {
            const OptionSpec *target = schema.find(spec->replacedBy);
            if (!target) {
                if (!spec->hasDefault || e.value.trimmed() != spec->defaultValue)
                    notes << QObject::tr("%1=%2 was dropped: the option is deprecated").arg(e.key, e.value);
                continue;
            }
            if (has(target->name)) {
                notes << QObject::tr("%1 was dropped: %2 is set").arg(e.key, target->name);
                continue;
            }
            ConfigEntry replaced = e;
            replaced.key = target->name;
            if (!spec->valueMap.isEmpty()) {
                QString key = e.value.trimmed().toLower();
                if (spec->type == OptionType::Bool && boolValue(e.value))
                    key = *boolValue(e.value) ? QStringLiteral("true") : QStringLiteral("false");
                if (!spec->valueMap.contains(key))
                    continue;  // this value needs no replacement
                replaced.value = spec->valueMap.value(key);
            } else if (spec->type != target->type || spec->bareLength != target->bareLength) {
                notes << QObject::tr("%1=%2 was dropped: use %3 instead").arg(e.key, e.value, target->name);
                continue;
            }
            notes << QObject::tr("%1=%2 became %3=%4").arg(e.key, e.value, replaced.key, replaced.value);
            out.append(replaced);
            continue;
        }
        if (e.key == "offset" && !e.disabled && !has("mill-diameters")) {
            // pcb2gcode reads --offset as the tool radius when --mill-diameters isn't given.
            const QString diameter = doubledLength(e.value);
            if (!diameter.isEmpty()) {
                if (units::parse(e.value, units::Dimension::Length, true)->value != 0) {
                    out.append({QStringLiteral("mill-diameters"), diameter, false, e.section});
                    notes << QObject::tr("offset=%1 (the tool radius) became mill-diameters=%2").arg(e.value, diameter);
                }
                continue;
            }
        }
        out.append(e);
    }
    entries = out;
    return notes;
}
