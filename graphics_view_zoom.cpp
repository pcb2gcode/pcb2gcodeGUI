/*
 * Copyright (c) 2016 Nicola Corna (nicola@corna.info)
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
 *
 *
 * Based on:
 * http://stackoverflow.com/a/19114517
 *
 */

#include "graphics_view_zoom.h"
#include <QMouseEvent>
#include <QApplication>
#include <QScrollBar>
#include <qmath.h>

Graphics_view_zoom::Graphics_view_zoom(QGraphicsView* view, Qt::KeyboardModifiers modifiers, double zoom_factor_base)
  : QObject(view), gview(view), modifiers(modifiers), zoom_factor_base(zoom_factor_base)
{
    gview->viewport()->installEventFilter(this);
    gview->setMouseTracking(true);
}

void Graphics_view_zoom::gentle_zoom(double factor)
{
    gview->scale(factor, factor);
    gview->centerOn(target_scene_pos);
    QPointF delta_viewport_pos = target_viewport_pos - QPointF(gview->viewport()->width() / 2.0,
                                                               gview->viewport()->height() / 2.0);
    QPointF viewport_center = gview->mapFromScene(target_scene_pos) - delta_viewport_pos;
    gview->centerOn(gview->mapToScene(viewport_center.toPoint()));
}

bool Graphics_view_zoom::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
        QPointF delta = target_viewport_pos - mouse_event->pos();

        if (qAbs(delta.x()) > 5 || qAbs(delta.y()) > 5)
        {
            target_viewport_pos = mouse_event->pos();
            target_scene_pos = gview->mapToScene(mouse_event->pos());
        }
    }
    else if (event->type() == QEvent::Wheel)
    {
        QWheelEvent* wheel_event = static_cast<QWheelEvent*>(event);
        if (QApplication::keyboardModifiers() == modifiers)
        {
            if (wheel_event->angleDelta().y() != 0) // Avoid zooming if wheel event is not vertical
            {
                double angle = wheel_event->angleDelta().y();
                double factor = qPow(zoom_factor_base, angle);
                gentle_zoom(factor);

                return true;
            }
        }
    }

    Q_UNUSED(object)
    return false;
}
