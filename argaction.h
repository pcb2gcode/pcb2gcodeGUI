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

#include <QMap>
#include <QString>
#include <QStringList>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QComboBox>

class argBase
{
public:
    virtual bool setValue(const QString)
    {
        return false;
    }

    virtual QString getValue()
    {
        return "";
    }

    virtual bool setEnabled(bool)
    {
        return false;
    }

    virtual bool getEnabled()
    {
        return false;
    }
};


class argDoubleSpinBox : public argBase
{
public:
    argDoubleSpinBox(QDoubleSpinBox *object) : object(object) {}

    bool setValue(const QString value);
    QString getValue();

    inline bool setEnabled(bool enabled)
    {
        object->setEnabled(enabled);
        return true;
    }

    inline bool getEnabled() {
        return object->isEnabled();
    }

protected:
    QDoubleSpinBox *object;
};


class argSpinBox : public argBase
{
public:
    argSpinBox(QSpinBox *object) : object(object) {}

    bool setValue(const QString value);
    QString getValue();

    inline bool setEnabled(bool enabled)
    {
        object->setEnabled(enabled);
        return true;
    }

    inline bool getEnabled()
    {
        return object->isEnabled();
    }

protected:
    QSpinBox *object;
};


class argCheckBox : public argBase
{
public:
    argCheckBox(QCheckBox *object) : object(object) {}

    bool setValue(const QString value);
    QString getValue();

    inline bool setEnabled(bool enabled)
    {
        object->setEnabled(enabled);
        return true;
    }

    inline bool getEnabled()
    {
        return object->isEnabled();
    }

protected:
    QCheckBox *object;
};


class argLineEdit : public argBase
{
public:
    argLineEdit(QLineEdit *object) : object(object) {}

    bool setValue(const QString value);
    QString getValue();

    inline bool setEnabled(bool enabled)
    {
        object->setEnabled(enabled);
        return true;
    }

    inline bool getEnabled()
    {
        return object->isEnabled();
    }

protected:
    QLineEdit *object;
};


class argComboBox : public argBase
{
public:
    argComboBox(QComboBox *object) : object(object) {}

    bool setValue(const QString value);
    QString getValue();

    inline bool setEnabled(bool enabled)
    {
        object->setEnabled(enabled);
        return true;
    }

    inline bool getEnabled()
    {
        return object->isEnabled();
    }

protected:
    QComboBox *object;
};


class argRadioButtonPair : public argBase
{
public:
    argRadioButtonPair(QPair<QRadioButton *, QRadioButton *> object) : object(object) {}

    bool setValue(const QString value);
    QString getValue();
    bool setEnabled(bool enabled);
    bool getEnabled();

protected:
    QPair<QRadioButton *, QRadioButton *> object;
};


class argAction {
public:
    QStringList getAllArgs(const QString prepend, bool getCommentedOptions);

    inline bool setValue(const QString key, const QString value)
    {
        return objects.value(key, new argBase)->setValue(value);
    }

    inline bool setEnabled(const QString key, const bool enabled)
    {
        return objects.value(key, new argBase)->setEnabled(enabled);
    }

    inline void insert(const QString argName, QDoubleSpinBox *doubleSpinBox)
    {
        objects.insert( argName, new argDoubleSpinBox(doubleSpinBox) );
    }

    inline void insert(const QString argName, QSpinBox *spinBox)
    {
        objects.insert( argName, new argSpinBox(spinBox) );
    }

    inline void insert(const QString argName, QCheckBox *checkBox)
    {
        objects.insert( argName, new argCheckBox(checkBox) );
    }

    inline void insert(const QString argName, QLineEdit *lineEdit)
    {
        objects.insert( argName, new argLineEdit(lineEdit) );
    }

    inline void insert(const QString argName, QComboBox *comboBox)
    {
        objects.insert( argName, new argComboBox(comboBox) );
    }

    inline void insert(const QString argName, QPair<QRadioButton *, QRadioButton *> radioButtonPair)
    {
        objects.insert( argName, new argRadioButtonPair(radioButtonPair) );
    }

protected:
    QMap<QString, argBase *> objects;
};

#endif // ARGACTION_H
