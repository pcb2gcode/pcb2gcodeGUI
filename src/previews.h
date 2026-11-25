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

#ifndef PREVIEWS_H
#define PREVIEWS_H

#include <QList>
#include <QString>

// An SVG image written by pcb2gcode.
struct PreviewImage {
    QString path;
    QString layer;  // front, back, outline, drill, milldrill
    QString stage;  // original, masked, processed, traced, contentions
    bool final = false;
    int toolIndex = -1;
    double toolDiameterMm = -1;
    QString linesWidth;  // for --draw-gerber-lines images
    int layerToolCount = 1;
    bool hasFinalTwin = false;  // a "final" image exists for the same layer, stage and tool

    QString label() const;
};

// Reads the list written with --manifest (see pcb2gcode's documentation of the format).
QList<PreviewImage> previewsFromManifest(const QString &manifestPath, QString *error = nullptr);
// Recognises pcb2gcode's SVG file names in `directory`.
QList<PreviewImage> previewsFromFileNames(const QString &directory);
// The manifest if it exists, the file names otherwise; sorted for display.
QList<PreviewImage> collectPreviews(const QString &directory, const QString &manifestPath = QString(),
                                    bool *fromManifest = nullptr);

void sortPreviews(QList<PreviewImage> &images);

#endif  // PREVIEWS_H
