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

#include "previewpane.h"

#include "graphics_view_zoom.h"

#include <QComboBox>
#include <QEvent>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMovie>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

PreviewPane::PreviewPane(QWidget *parent) : QWidget(parent)
{
    m_combo = new QComboBox;
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_combo->setMinimumContentsLength(24);
    m_combo->setEnabled(false);
    m_combo->setPlaceholderText(tr("No preview yet"));
    connect(m_combo, &QComboBox::currentIndexChanged, this, &PreviewPane::showImage);

    m_fit = new QToolButton;
    m_fit->setText(tr("Fit"));
    m_fit->setToolTip(tr("Fit the board in the view (mouse wheel zooms)"));
    connect(m_fit, &QToolButton::clicked, this, &PreviewPane::fit);

    m_refresh = new QToolButton;
    m_refresh->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refresh->setToolTip(tr("Refresh the preview (F5)"));
    connect(m_refresh, &QToolButton::clicked, this, &PreviewPane::refreshRequested);

    m_movie = new QMovie(QStringLiteral(":/images/loading.gif"), QByteArray(), this);
    m_spinner = new QLabel;
    m_spinner->setMovie(m_movie);
    m_spinner->setVisible(false);

    auto *top = new QHBoxLayout;
    top->addWidget(m_combo, 1);
    top->addWidget(m_spinner);
    top->addWidget(m_fit);
    top->addWidget(m_refresh);

    m_message = new QLabel;
    m_message->setWordWrap(true);
    m_message->setVisible(false);
    m_detailsButton = new QToolButton;
    m_detailsButton->setText(tr("Details…"));
    m_detailsButton->setVisible(false);
    connect(m_detailsButton, &QToolButton::clicked, this, [this] {
        QMessageBox box(QMessageBox::Information, tr("pcb2gcode output"), m_message->text(), QMessageBox::Close,
                        this);
        box.setDetailedText(m_details);
        box.exec();
    });
    auto *messageRow = new QHBoxLayout;
    messageRow->addWidget(m_message, 1);
    messageRow->addWidget(m_detailsButton, 0, Qt::AlignTop);

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_view->setBackgroundBrush(QColor(0xf4, 0xf4, 0xf4));
    new Graphics_view_zoom(m_view);
    // Installed after the zoom helper so that it sees wheel events first.
    m_view->viewport()->installEventFilter(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(top);
    layout->addLayout(messageRow);
    layout->addWidget(m_view, 1);
}

QString PreviewPane::currentLabel() const
{
    return m_combo->currentText();
}

void PreviewPane::setImages(const QList<PreviewImage> &images, const QString &source)
{
    const QString previous = m_combo->currentText();
    m_images = images;
    {
        const QSignalBlocker blocker(m_combo);
        m_combo->clear();
        for (const PreviewImage &image : images)
            m_combo->addItem(image.label(), image.path);
    }
    m_combo->setEnabled(!images.isEmpty());
    if (images.isEmpty()) {
        m_scene->clear();
        setMessage(tr("pcb2gcode didn't write any preview images."));
        return;
    }
    const int keep = m_combo->findText(previous);
    m_combo->setCurrentIndex(keep >= 0 ? keep : 0);
    showImage(m_combo->currentIndex());
    setMessage(source);
}

void PreviewPane::setBusy(bool busy)
{
    m_spinner->setVisible(busy);
    if (busy)
        m_movie->start();
    else
        m_movie->stop();
}

void PreviewPane::setMessage(const QString &text, bool error, const QString &details)
{
    m_message->setText(text);
    m_message->setVisible(!text.isEmpty());
    m_message->setStyleSheet(error ? QStringLiteral("color: #b02020;") : QString());
    m_details = details;
    m_detailsButton->setVisible(!details.isEmpty());
}

void PreviewPane::showImage(int index)
{
    m_scene->clear();
    if (index < 0 || index >= m_images.size())
        return;
    auto *item = new QGraphicsSvgItem(m_images.at(index).path);
    m_scene->addItem(item);
    const QRectF rect = item->boundingRect();
    m_scene->setSceneRect(rect);
    // Keep the zoom while flipping between images of the same board.
    if (m_lastRect.isNull() || std::abs(rect.width() - m_lastRect.width()) > 1e-6 * rect.width() ||
        std::abs(rect.height() - m_lastRect.height()) > 1e-6 * rect.height())
        fit();
    m_lastRect = rect;
}

void PreviewPane::fit()
{
    m_autoFit = true;
    if (!m_scene->items().isEmpty())
        m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

bool PreviewPane::eventFilter(QObject *object, QEvent *event)
{
    // The board stays fitted to the view until the user zooms.
    if (event->type() == QEvent::Wheel)
        m_autoFit = false;
    else if (event->type() == QEvent::Resize && m_autoFit && !m_scene->items().isEmpty())
        QTimer::singleShot(0, this, [this] { m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio); });
    return QWidget::eventFilter(object, event);
}
