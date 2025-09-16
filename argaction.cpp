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

#include "argaction.h"

template<>
bool argBase<QDoubleSpinBox>::setValue(const QString value)
{
    bool ok;
    const double numericalValue = value.toDouble(&ok);

    if (ok)
        object->setValue(numericalValue);

    return ok;
}

template<>
QString argBase<QDoubleSpinBox>::getValue()
{
    return QString::number(object->value(), 'f', 4);
}

template<>
bool argBase<QSpinBox>::setValue(const QString value)
{
    bool ok;
    double numericalValue = value.toInt(&ok);

    if (ok)
        object->setValue(numericalValue);

    return ok;
}

template<>
QString argBase<QSpinBox>::getValue()
{
    return QString::number(object->value());
}

template<>
bool argBase<QCheckBox>::setValue(const QString value)
{
    if(value == "1" || value.compare("true", Qt::CaseInsensitive) == 0)
    {
        object->setChecked(true);
        return true;
    }
    else if (value == "0" || value.compare("false", Qt::CaseInsensitive) == 0)
    {
        object->setChecked(false);
        return true;
    }
    else
        return false;
}

template<>
QString argBase<QCheckBox>::getValue()
{
    if(object->isChecked())
        return "true";
    else
        return "false";
}

template<>
bool argBase<QLineEdit>::setValue(const QString value)
{
    object->setText(value);
    return true;
}

template<>
QString argBase<QLineEdit>::getValue()
{
    return object->text();
}

template<>
bool argBase<QComboBox>::setValue(const QString value)
{
    const int index = object->findText(value, Qt::MatchFixedString);

    if( index >= 0 )
    {
        object->setCurrentIndex(index);
        return true;
    }
    else
        return false;
}

template<>
QString argBase<QComboBox>::getValue()
{
    return object->currentText();
}

template<>
bool argBase<QButtonGroup>::setValue(const QString value)
{
    if(value == "1" || value.compare("true", Qt::CaseInsensitive) == 0)
    {
        object->button(0)->setChecked(true);
        return true;
    }
    else if(value == "0" || value.compare("false", Qt::CaseInsensitive) == 0)
    {
        object->button(1)->setChecked(true);
        return true;
    }
    else
        return false;
}

template<>
QString argBase<QButtonGroup>::getValue()
{
    if(object->checkedId() == 0)
        return "true";
    else
        return "false";
}

template<>
void argBase<QButtonGroup>::setEnabled(bool enabled)
{
    QList<QAbstractButton *> buttons = object->buttons();

    foreach (QAbstractButton *button, buttons)
    {
        button->setEnabled(enabled);
    }
}

template<>
bool argBase<QButtonGroup>::getEnabled()
{
    return object->button(0)->isEnabled();
}

QStringList argAction::getAllArgs(bool getCommentedOptions)
{
    QStringList output;
    QString value;
    const QString prepend = getCommentedOptions ? "" : "--";

    for (QMap<QString, argBaseVirtual *>::const_iterator i = objects.constBegin();
            i != objects.constEnd(); i++)
    {
        value = i.value()->getValue();
        if ( !value.isEmpty() && !isDefault(i.key()))
        {
            if( i.value()->getEnabled() )
                output << prepend + i.key() + '=' + value;
            else
                if(getCommentedOptions)
                    output << prepend + "#@#" + i.key() + '=' + value;
        }
    }

    return output;
}
