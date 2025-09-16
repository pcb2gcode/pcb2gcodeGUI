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

#ifndef ARGACTION_H
#define ARGACTION_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>

class argBaseVirtual
{
public:
    virtual bool setValue(const QString) = 0;
    virtual QString getValue() = 0;
    virtual void setEnabled(bool) = 0;
    virtual bool getEnabled() = 0;
};

template <typename T>
class argBase : public argBaseVirtual
{
protected:
    T *object;

public:
    argBase(T *object) : object(object) {}

    bool setValue(const QString);
    QString getValue();

    inline void setEnabled(bool enabled)
    {
        object->setEnabled(enabled);
    }

    inline bool getEnabled()
    {
        return object->isEnabled();
    }
};

template <> bool argBase<QDoubleSpinBox>::setValue(const QString);
template <> QString argBase<QDoubleSpinBox>::getValue();

template <> bool argBase<QSpinBox>::setValue(const QString);
template <> QString argBase<QSpinBox>::getValue();

template <> bool argBase<QCheckBox>::setValue(const QString);
template <> QString argBase<QCheckBox>::getValue();

template <> bool argBase<QComboBox>::setValue(const QString);
template <> QString argBase<QComboBox>::getValue();

template <> bool argBase<QLineEdit>::setValue(const QString);
template <> QString argBase<QLineEdit>::getValue();

template <> bool argBase<QButtonGroup>::setValue(const QString);
template <> QString argBase<QButtonGroup>::getValue();
template <> void argBase<QButtonGroup>::setEnabled(bool enabled);
template <> bool argBase<QButtonGroup>::getEnabled();

class argAction {
public:
    QStringList getAllArgs(bool getCommentedOptions=false);

    inline bool setValue(const QString key, const QString value)
    {
        QMap<QString, argBaseVirtual *>::iterator pos = objects.find(key);

        if (pos != objects.end())
            return (*pos)->setValue(value);
        else
            return false;
    }

    inline void setEnabled(const QString key, const bool enabled)
    {
        objects.value(key)->setEnabled(enabled);
    }

    template <typename T>
    inline void insert(const QString argName, T *object, QString defaultValue1="", QString defaultValue2="")
    {
        objects.insert(argName, new argBase<T>(object));
        if(defaultValue1.length() > 0) 
        {
            defaultValues1.insert(argName, defaultValue1);
            if(defaultValue2.length() > 0)
                defaultValues2.insert(argName, defaultValue2);
        }
    }

    inline bool isDefault(const QString key)
    {
        return objects.value(key)->getValue() == defaultValues1.value(key) || objects.value(key)->getValue() == defaultValues2.value(key);
    }

protected:
    QMap<QString, argBaseVirtual *> objects;
    QMap<QString, QString> defaultValues1;
    QMap<QString, QString> defaultValues2;
};

#endif // ARGACTION_H
