/*
 * Copyright (c) 2015 Nicola Corna (nicola@corna.info)
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

#ifndef OUTPUTWINDOW_H
#define OUTPUTWINDOW_H

#include <QDialog>
#include <QPushButton>
#include <QPlainTextEdit>

namespace Ui {
class outputWindow;
}

class outputWindow : public QDialog
{
    Q_OBJECT

public:
    explicit outputWindow(QWidget *parent = 0);
    ~outputWindow();
    QPushButton *getPushButton();
    QPlainTextEdit *getPlainTextEdit();

private:
    Ui::outputWindow *ui;
};

#endif // OUTPUTWINDOW_H
