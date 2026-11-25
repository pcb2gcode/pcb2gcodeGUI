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

#ifndef PREVIEWPANE_H
#define PREVIEWPANE_H

#include "previews.h"

#include <QWidget>

class QComboBox;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QMovie;
class QToolButton;

class PreviewPane : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPane(QWidget *parent = nullptr);

    void setImages(const QList<PreviewImage> &images, const QString &source);
    void setBusy(bool busy);
    void setMessage(const QString &text, bool error = false, const QString &details = QString());
    const QList<PreviewImage> &images() const { return m_images; }
    QString currentLabel() const;

signals:
    void refreshRequested();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void showImage(int index);
    void fit();

    QComboBox *m_combo;
    QToolButton *m_refresh;
    QToolButton *m_fit;
    QLabel *m_spinner;
    QMovie *m_movie;
    QLabel *m_message;
    QToolButton *m_detailsButton;
    QString m_details;
    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QList<PreviewImage> m_images;
    QRectF m_lastRect;
    bool m_autoFit = true;
};

#endif  // PREVIEWPANE_H
