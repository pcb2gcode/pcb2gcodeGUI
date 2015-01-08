#include "argaction.h"

argAction::argAction()
{

}

argAction::~argAction()
{

}

bool argAction::setDoubleSpinBox(void *doubleSpinBox, QString argValue)
{
    bool ok;
    double value = argValue.toDouble(&ok);

    if (ok)
        static_cast<QDoubleSpinBox *>(doubleSpinBox)->setValue(value);

    return ok;
}

bool argAction::setSpinBox(void *spinBox, QString argValue)
{
    bool ok;
    int value = argValue.toInt(&ok, 10);

    if (ok)
        static_cast<QSpinBox *>(spinBox)->setValue(value);

    return ok;
}

bool argAction::setCheckBox(void *checkBox, QString argValue)
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

bool argAction::setLineEdit(void *lineEdit, QString argValue)
{
    static_cast<QLineEdit *>(lineEdit)->setText(argValue);

    return true;
}

bool argAction::setComboBox(void *comboBox, QString argValue)
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

bool argAction::setButtonGroup(void *buttonGroup, QString argValue)
{
    if(argValue == "1" || argValue.compare("true", Qt::CaseInsensitive) == 0)
    {
        static_cast<QButtonGroup *>(buttonGroup)->button(0)->setChecked(true);
        return true;
    }
    else if(argValue == "0" || argValue.compare("false", Qt::CaseInsensitive) == 0)
    {
        static_cast<QButtonGroup *>(buttonGroup)->button(1)->setChecked(true);
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

QString argAction::getButtonGroup(void *buttonGroup)
{
    if(static_cast<QButtonGroup *>(buttonGroup)->button(0)->isChecked())
        return "true";
    else
        return "false";
}

bool argAction::setValue(QString key, QString value)
{
    argElement element;

    if( argList.contains(key) )
    {
        element = argList.value(key);
        return element.setFunction( element.object, value );
    }

    return false;
}

QStringList argAction::getAllArgs(bool getCommentedOptions, bool getDisabledObjects)
{
    QStringList output;
    QString value;

    for(QHash<QString, argElement>::const_iterator i = argList.constBegin(); i != argList.constEnd(); i++)
    {
        if( getDisabledObjects || static_cast<QWidget *>(i.value().object)->isEnabled() )
        {
            value = i.value().getFunction( i.value().object );
            if ( !value.isEmpty() )
            {
                if(i.value().commentedOption)
                {
                    if(getCommentedOptions)
                        output << "#@!" + i.key() + '=' + value;
                }
                else
                    output << i.key() + '=' + value;
            }
        }
    }

    return output;
}
