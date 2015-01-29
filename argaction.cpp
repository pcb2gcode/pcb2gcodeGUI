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

argAction::argAction()
{

}

argAction::~argAction()
{

}

bool argAction::setDoubleSpinBox(void *doubleSpinBox, const QString argValue)
{
    bool ok;
    double value = argValue.toDouble(&ok);

    if (ok)
        static_cast<QDoubleSpinBox *>(doubleSpinBox)->setValue(value);

    return ok;
}

bool argAction::setSpinBox(void *spinBox, const QString argValue)
{
    bool ok;
    int value = argValue.toInt(&ok, 10);

    if (ok)
        static_cast<QSpinBox *>(spinBox)->setValue(value);

    return ok;
}

bool argAction::setCheckBox(void *checkBox, const QString argValue)
{
    if(argValue == "1" || argValue.compare("true", Qt::CaseInsensitive) == 0)
    {
        static_cast<QCheckBox *>(checkBox)->setChecked(true);
        return true;
    }
    else if (argValue == "0" || argValue.compare("false", Qt::CaseInsensitive) == 0)
    {
        static_cast<QCheckBox *>(checkBox)->setChecked(false);
        return true;
    }
    else
        return false;
}

bool argAction::setLineEdit(void *lineEdit, const QString argValue)
{
    static_cast<QLineEdit *>(lineEdit)->setText(argValue);

    return true;
}

bool argAction::setComboBox(void *comboBox, const QString argValue)
{
    int index = static_cast<QComboBox *>(comboBox)->findText(argValue, Qt::MatchFixedString);

    if( index >= 0 )
    {
        static_cast<QComboBox *>(comboBox)->setCurrentIndex(index);
        return true;
    }
    else
        return false;
}

bool argAction::setRadioButtonPair(void *radioButtonPair, const QString argValue)
{
    if(argValue == "1" || argValue.compare("true", Qt::CaseInsensitive) == 0)
    {
        static_cast<QWidgetPair<QRadioButton, QRadioButton> *>(radioButtonPair)->object1->setChecked(true);
        return true;
    }
    else if(argValue == "0" || argValue.compare("false", Qt::CaseInsensitive) == 0)
    {
        static_cast<QWidgetPair<QRadioButton, QRadioButton> *>(radioButtonPair)->object2->setChecked(true);
        return true;
    }
    else
        return false;
}

QString argAction::getDoubleSpinBox(void *doubleSpinBox)
{
    return QString::number(static_cast<QDoubleSpinBox *>(doubleSpinBox)->value(), 'f', 4);
}

QString argAction::getSpinBox(void *spinBox)
{
    return QString::number(static_cast<QSpinBox *>(spinBox)->value(), 10);
}

QString argAction::getCheckBox(void *checkBox)
{
    if(static_cast<QCheckBox *>(checkBox)->isChecked())
        return "true";
    else
        return "false";
}

QString argAction::getLineEdit(void *lineEdit)
{
    return static_cast<QLineEdit *>(lineEdit)->text();
}

QString argAction::getComboBox(void *comboBox)
{
    return static_cast<QComboBox *>(comboBox)->currentText();
}

QString argAction::getRadioButtonPair(void *radioButtonPair)
{
    if(static_cast<QWidgetPair<QRadioButton, QRadioButton> *>(radioButtonPair)->object1->isChecked())
        return "true";
    else
        return "false";
}

bool argAction::setValue(const QString key, const QString value)
{
    argElement element;

    if( argList.contains(key) )
    {
        element = argList.value(key);
        return element.setFunction( element.object, value );
    }

    return false;
}

QStringList argAction::getAllArgs(bool getCommentedOptions)
{
    QStringList output;
    QString value;

    for(QMap<QString, argElement>::const_iterator i = argList.constBegin(); i != argList.constEnd(); i++)
    {
        value = i.value().getFunction( i.value().object );
        if ( !value.isEmpty() )
        {
#ifndef Q_OS_WIN        //Crashes if quotation marks are present on Windows... Don't know why... FIXME
            if( stringMarks && i.value().getFunction == &getLineEdit )
                value = '\"' + value + '\"';
#endif

            if( static_cast<QWidget *>(i.value().object)->isEnabled() )
                output << i.key() + '=' + value;
            else
                if(getCommentedOptions)
                    output << "#@#" + i.key() + '=' + value;
        }
    }

    return output;
}

bool argAction::setEnabled(const QString key, const bool enabled)
{
    if( argList.contains(key) )
    {
        static_cast<QWidget *>(argList.value(key).object)->setEnabled(enabled);
        return true;
    }
    else
        return false;
}
