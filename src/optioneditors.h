/*
 * Copyright (c) 2026 pcb2gcodeGUI contributors
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

#ifndef OPTIONEDITORS_H
#define OPTIONEDITORS_H

#include "layout.h"
#include "optionschema.h"

#include <QDoubleSpinBox>
#include <QObject>
#include <QStringList>
#include <optional>

class QAbstractButton;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QWidget;

class EditorContext
{
public:
    virtual ~EditorContext() = default;
    virtual bool isMetric() const = 0;
    virtual QString browseDirectory(PathKind kind) const = 0;
    virtual void rememberDirectory(PathKind kind, const QString &directory) = 0;
};

// A QDoubleSpinBox that shows numbers without trailing zeros.
class QuantitySpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    using QDoubleSpinBox::QDoubleSpinBox;

protected:
    QString textFromValue(double value) const override;
};

// The editor of one pcb2gcode option: a label and a field for a QFormLayout.
//
// values() is what goes after "--name=". It's empty when the option is unset, which
// happens for empty text fields, for "(not set)" choices and for unticked override boxes,
// the check box in front of optional values that have no pcb2gcode default.
class OptionEditor : public QObject
{
    Q_OBJECT
public:
    static OptionEditor *create(const OptionSpec &spec, const OptionHint &hint,
                                EditorContext *context, QWidget *parent);

    const OptionSpec &spec() const { return m_spec; }
    const OptionHint &hint() const { return m_hint; }
    QString name() const { return m_spec.name; }
    QLabel *label() const { return m_label; }
    QWidget *field() const { return m_field; }

    QStringList values() const;
    // The value seen by conditions: the set value, else pcb2gcode's default.
    std::optional<QString> effectiveValue() const;
    // True when there's nothing to pass: unset, or equal to pcb2gcode's default.
    bool isDefault() const;
    QString problem() const;

    // Loads values in pcb2gcode syntax; `metric` tells how bare numbers are meant.
    bool setValues(const QStringList &values, bool metric, QString *error = nullptr);
    // Loads a value but leaves an optional value switched off.
    bool setInactiveValues(const QStringList &values, bool metric, QString *error = nullptr);
    void reset(bool usePreset);
    void unitSystemChanged(bool metric);

    bool hasOverride() const { return m_override != nullptr; }
    bool isOverridden() const;
    void setOverridden(bool on);

    void setRuleEnabled(bool enabled);
    bool ruleEnabled() const { return m_ruleEnabled; }
    // `usedWhen` describes when pcb2gcode uses the option, e.g. "front or back is set".
    void setUsed(bool used, const QString &usedWhen);
    bool isUsed() const { return m_used; }
    void setHighlighted(bool changed);

signals:
    void changed();
    void fileChosen();

protected:
    OptionEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *context,
                 QWidget *parent);

    virtual QStringList editorValues() const = 0;
    virtual bool loadValues(const QStringList &values, bool metric, QString *error) = 0;
    virtual void applyUnits(bool metric) { Q_UNUSED(metric) }
    virtual QString editorProblem() const { return QString(); }
    virtual bool sameAsDefault(const QStringList &values) const;

    void setEditor(QWidget *editor, QWidget *extra = nullptr);
    // Shows the label as the button's own text, as for check boxes.
    void moveLabelOnto(QAbstractButton *button);
    void emitChanged();
    QString startValue(bool usePreset) const;
    bool isMetric() const { return m_context->isMetric(); }

    OptionSpec m_spec;
    OptionHint m_hint;
    EditorContext *m_context;
    bool m_loading = false;

private:
    void updateTooltips();
    void updateEnabled();
    void showResetMenu(const QPoint &globalPos);

    QLabel *m_label;
    QWidget *m_textWidget;  // where the option's name is shown: the label or a check box
    QWidget *m_field;
    QHBoxLayout *m_fieldLayout;
    QWidget *m_editor = nullptr;
    QCheckBox *m_override = nullptr;
    QLabel *m_note = nullptr;
    bool m_ruleEnabled = true;
    bool m_used = true;
    QString m_usedWhen;
};

// The label shown for an option without a layout hint, e.g. "mill-feed" -> "Mill feed".
QString humanizeOptionName(const QString &name);

#endif  // OPTIONEDITORS_H
