/*
 * Copyright (c) 2015-2016 Nicola Corna (nicola@corna.info)
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

#ifndef CMDLINEARGS_H
#define CMDLINEARGS_H

#include <QDialog>

namespace Ui {
class CmdLineArgs;
}

class CmdLineArgs : public QDialog
{
    Q_OBJECT

public:
    explicit CmdLineArgs(QWidget *parent = 0);
    void setText(QString text);
    ~CmdLineArgs();

private:
    Ui::CmdLineArgs *ui;
};

#endif // CMDLINEARGS_H
