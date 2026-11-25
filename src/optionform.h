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

#ifndef OPTIONFORM_H
#define OPTIONFORM_H

#include "condition.h"
#include "configfile.h"
#include "layout.h"
#include "optioneditors.h"
#include "optionschema.h"

#include <QHash>
#include <QWidget>

class QGroupBox;
class QLabel;
class QTabWidget;

// The option tabs, built from an OptionSchema and a Layout.
class OptionForm : public QWidget, public EditorContext
{
    Q_OBJECT
public:
    explicit OptionForm(QWidget *parent = nullptr);

    void build(const OptionSchema &schema, const Layout &layout);
    const OptionSchema &schema() const { return m_schema; }

    OptionEditor *editor(const QString &name) const { return m_editors.value(name); }
    const QList<OptionEditor *> &editors() const { return m_order; }
    // Whether the pcb2gcode in use has this option, shown or not (e.g. "manifest").
    bool supports(const QString &option) const;

    // The options to pass to pcb2gcode, as --name=value, in the order they are shown.
    QStringList arguments() const;
    struct Problem {
        QString message;
        QStringList options;
    };
    // What pcb2gcode would reject: fields that don't parse and failing rules of the schema.
    QList<Problem> findProblems() const;
    QStringList problems() const;

    ConfigFile toConfig() const;
    // Loads a configuration file on top of pcb2gcode's defaults. Relative input paths are
    // resolved against `baseDirectory`. Returns notes about anything that wasn't loaded as is.
    QStringList applyConfig(const ConfigFile &config, const QString &baseDirectory = QString());
    void resetAll(bool usePresets);
    bool setOptionValues(const QString &name, const QStringList &values, QString *error = nullptr);

    bool metric() const override;
    QString browseDirectory(PathKind kind) const override;
    void rememberDirectory(PathKind kind, const QString &directory) override;

    bool isUsed(const OptionEditor *editor) const;
    condition::Context conditionContext() const;

signals:
    void changed();
    void fileChosen();

private:
    struct Section {
        QGroupBox *box;
        QLabel *note;
        QList<OptionEditor *> editors;
    };
    struct Tab {
        QWidget *page;
        QWidget *content;
        QString title;
        int trailingItems = 0;  // layout items below the sections: a stretch and the wiki link
        QList<Section> sections;
    };

    Tab &createTab(const QString &id, const QString &title, const QString &wiki);
    void addSection(Tab &tab, const QString &title, const QList<const OptionSpec *> &specs);
    void onEditorChanged(OptionEditor *editor);
    void applyUnitSystem();
    void refresh();

    OptionSchema m_schema;
    Layout m_layout;
    QTabWidget *m_tabs;
    QList<Tab> m_tabList;
    QHash<QString, int> m_tabIndex;
    QHash<QString, OptionEditor *> m_editors;
    QHash<const OptionEditor *, QString> m_tabTitleOf;
    QList<OptionEditor *> m_order;
    QHash<int, QString> m_directories;
    bool m_building = false;
};

#endif  // OPTIONFORM_H
