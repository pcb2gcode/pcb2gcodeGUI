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

bool isTrue(const QString &value)
{
    const QString v = value.trimmed().toLower();
    return v.isEmpty() || v == "1" || v == "true" || v == "yes" || v == "on";
}

// Doubles a length while keeping its unit: "0.1mm" -> "0.2mm", "0.004" -> "0.008".
QString doubledLength(const QString &value)
{
    const auto q = units::parse(value, units::Dimension::Length, true);
    if (!q)
        return QString();
    return units::formatNumber(q->value * 2, 8) + q->unit.simplified().remove(' ');
}

bool isLegacy(const QString &key, const OptionSchema &schema)
{
    const OptionSpec *spec = schema.find(key);
    return !spec || spec->deprecated || spec->unsupported;
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
        ConfigEntry entry = e;
        if ((e.key == "drill-front" || e.key == "cut-front") && isLegacy(e.key, schema)) {
            const QString side = e.key == "drill-front" ? "drill-side" : "cut-side";
            if (!has(side)) {
                entry.key = side;
                entry.value = isTrue(e.value) ? "front" : "back";
                out.append(entry);
                notes << QObject::tr("%1=%2 became %3=%4").arg(e.key, e.value, side, entry.value);
            }
            continue;
        }
        if (e.key == "milldrill" && isLegacy(e.key, schema)) {
            if (isTrue(e.value) && !has("min-milldrill-hole-diameter")) {
                entry.key = "min-milldrill-hole-diameter";
                entry.value = "0";
                out.append(entry);
                notes << QObject::tr("milldrill=true became min-milldrill-hole-diameter=0");
            }
            continue;
        }
        if (e.key == "g64" && isLegacy(e.key, schema)) {
            if (!has("tolerance")) {
                entry.key = "tolerance";
                out.append(entry);
                notes << QObject::tr("g64 became tolerance");
            }
            continue;
        }
        if (e.key == "offset" && !has("mill-diameters") && !e.disabled) {
            // pcb2gcode reads --offset as the tool radius when --mill-diameters isn't given.
            const QString diameter = doubledLength(e.value);
            if (!diameter.isEmpty()) {
                if (units::parse(e.value, units::Dimension::Length, true)->value != 0) {
                    entry.key = "mill-diameters";
                    entry.value = diameter;
                    out.append(entry);
                    notes << QObject::tr("offset=%1 (the tool radius) became mill-diameters=%2")
                                 .arg(e.value, diameter);
                }
                continue;
            }
        }
        if ((e.key == "mirror-absolute" || e.key == "svg" || e.key == "vectorial") &&
            (isLegacy(e.key, schema) || e.key == "vectorial")) {
            if (e.key == "vectorial" && !isTrue(e.value))
                notes << QObject::tr("vectorial=false was removed: pcb2gcode always uses the vectorial engine");
            continue;
        }
        if (e.key == "extra-passes" && isLegacy(e.key, schema)) {
            notes << QObject::tr("extra-passes=%1 was removed: use the minimum isolation width instead")
                         .arg(e.value);
            continue;
        }
        out.append(entry);
    }
    entries = out;
    return notes;
}
