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

#include "argaction.h"

bool argDoubleSpinBox::setValue(const QString value)
{
    bool ok;
    const double numericalValue = value.toDouble(&ok);

    if (ok)
        object->setValue(numericalValue);

    return ok;
}

QString argDoubleSpinBox::getValue()
{
    return QString::number(object->value(), 'f', 4);
}

bool argSpinBox::setValue(const QString value)
{
    bool ok;
    double numericalValue = value.toInt(&ok);

    if (ok)
        object->setValue(numericalValue);

    return ok;
}

QString argSpinBox::getValue()
{
    return QString::number(object->value());
}

bool argCheckBox::setValue(const QString value)
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

QString argCheckBox::getValue()
{
    if(object->isChecked())
        return "true";
    else
        return "false";
}

bool argLineEdit::setValue(const QString value)
{
    object->setText(value);
    return true;
}

QString argLineEdit::getValue()
{
    return object->text();
}

bool argComboBox::setValue(const QString value)
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

QString argComboBox::getValue()
{
    return object->currentText();
}

bool argRadioButtonPair::setValue(const QString value)
{
    if(value == "1" || value.compare("true", Qt::CaseInsensitive) == 0)
    {
        object.first->setChecked(true);
        object.second->setChecked(false);
        return true;
    }
    else if(value == "0" || value.compare("false", Qt::CaseInsensitive) == 0)
    {
        object.first->setChecked(false);
        object.second->setChecked(true);
        return true;
    }
    else
        return false;
}

QString argRadioButtonPair::getValue()
{
    if(object.first->isChecked())
        return "true";
    else
        return "false";
}

bool argRadioButtonPair::setEnabled(bool enabled)
{
    object.first->setEnabled(enabled);
    object.second->setEnabled(enabled);

    return true;
}

bool argRadioButtonPair::getEnabled()
{
    return object.first->isEnabled();
}

QStringList argAction::getAllArgs(const QString prepend, bool getCommentedOptions)
{
    QStringList output;
    QString value;

    for(QMap<QString, argBase *>::const_iterator i = objects.constBegin(); i != objects.constEnd(); i++)
    {
        value = i.value()->getValue();
        if ( !value.isEmpty() )
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
