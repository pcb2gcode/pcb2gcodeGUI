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

    static bool setDoubleSpinBox(void *doubleSpinBox, QString argValue);
    static bool setSpinBox(void *spinBox, QString argValue);
    static bool setCheckBox(void *checkBox, QString argValue);
    static bool setLineEdit(void *lineEdit, QString argValue);
    static bool setComboBox(void *comboBox, QString argValue);
    static bool setButtonGroup(void *buttonGroup, QString argValue);
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

    bool setValue(QString key, QString value);

    QStringList getAllArgs(bool getCommentedOptions = false, bool getDisabledObjects = false);

    inline void insert( QString argName, QDoubleSpinBox *doubleSpinBox, bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setDoubleSpinBox, .getFunction = &getDoubleSpinBox,
                                               .object = doubleSpinBox, .commentedOption = commentedOption } );
    }

    inline void insert( QString argName, QSpinBox *spinBox, bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setSpinBox, .getFunction = &getSpinBox,
                                               .object = spinBox, .commentedOption = commentedOption } );
    }

    inline void insert( QString argName, QCheckBox *checkBox, bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setCheckBox, .getFunction = &getCheckBox,
                                               .object = checkBox, .commentedOption = commentedOption } );
    }

    inline void insert( QString argName, QLineEdit *lineEdit, bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setLineEdit, .getFunction = &getLineEdit,
                                               .object = lineEdit, .commentedOption = commentedOption } );
    }

    inline void insert( QString argName, QComboBox *comboBox, bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setComboBox, .getFunction = &getComboBox,
                                               .object = comboBox, .commentedOption = commentedOption } );
    }

    inline void insert( QString argName, QButtonGroup *buttonGroup, bool commentedOption = false )
    {
        argList.insert( argName, (argElement){ .setFunction = &setButtonGroup, .getFunction = &getButtonGroup,
                                               .object = buttonGroup, .commentedOption = commentedOption } );
    }

};

#endif // ARGACTION_H
