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

#ifndef ARGACTION_H
#define ARGACTION_H

#include <QHash>
#include <QString>
#include <QStringList>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>

class argAction
{
protected:

    static bool setDoubleSpinBox(void *doubleSpinBox, const QString argValue);
    static bool setSpinBox(void *spinBox, const QString argValue);
    static bool setCheckBox(void *checkBox, const QString argValue);
    static bool setLineEdit(void *lineEdit, const QString argValue);
    static bool setComboBox(void *comboBox, const QString argValue);
    static bool setButtonGroup(void *buttonGroup, const QString argValue);
    static QString getDoubleSpinBox(void *doubleSpinBox);
    static QString getSpinBox(void *spinBox);
    static QString getCheckBox(void *checkBox);
    static QString getLineEdit(void *lineEdit);
    static QString getComboBox(void *comboBox);
    static QString getButtonGroup(void *buttonGroup);

    struct argElement
    {
        bool (*setFunction)(void *object, QString argValue);
        QString (*getFunction)(void *object);
        void *object;
        bool commentedOption;
    };

    QHash<QString,argElement> argList;

public:
    explicit argAction();
    ~argAction();

    bool setValue(const QString key, const QString value);

    QStringList getAllArgs(const bool getCommentedOptions = false, const bool getDisabledObjects = false);

    inline void insert( const QString argName, QDoubleSpinBox *doubleSpinBox, const bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setDoubleSpinBox, .getFunction = &getDoubleSpinBox,
                                               .object = doubleSpinBox, .commentedOption = commentedOption } );
    }

    inline void insert( const QString argName, QSpinBox *spinBox, const bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setSpinBox, .getFunction = &getSpinBox,
                                               .object = spinBox, .commentedOption = commentedOption } );
    }

    inline void insert( const QString argName, QCheckBox *checkBox, const bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setCheckBox, .getFunction = &getCheckBox,
                                               .object = checkBox, .commentedOption = commentedOption } );
    }

    inline void insert( const QString argName, QLineEdit *lineEdit, const bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setLineEdit, .getFunction = &getLineEdit,
                                               .object = lineEdit, .commentedOption = commentedOption } );
    }

    inline void insert( const QString argName, QComboBox *comboBox, const bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setComboBox, .getFunction = &getComboBox,
                                               .object = comboBox, .commentedOption = commentedOption } );
    }

    inline void insert( const QString argName, QButtonGroup *buttonGroup, const bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setButtonGroup, .getFunction = &getButtonGroup,
                                               .object = buttonGroup, .commentedOption = commentedOption } );
    }

};

#endif // ARGACTION_H
