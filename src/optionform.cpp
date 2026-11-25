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

#include "optionform.h"

#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QSet>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

const char *const kWikiBase = "https://github.com/pcb2gcode/pcb2gcode/wiki/";

bool shownInForm(const OptionSpec &spec, const Layout &layout)
{
    return spec.type != OptionType::None && !spec.cliOnly && !spec.deprecated && !spec.unsupported &&
           !layout.hidden.contains(spec.name);
}

// "Drilling options, for making holes in the PCB" -> "Drilling"
QString shortGroupTitle(const OptionGroup &group)
{
    QString t = group.title.section(',', 0, 0).trimmed();
    if (t.endsWith(QStringLiteral(" options"), Qt::CaseInsensitive))
        t.chop(8);
    if (t.isEmpty())
        t = group.id;
    if (!t.isEmpty())
        t[0] = t.at(0).toUpper();
    return t;
}

bool isInputPath(PathKind kind)
{
    return kind == PathKind::InputGerber || kind == PathKind::InputExcellon ||
           kind == PathKind::InputGcode || kind == PathKind::InputText || kind == PathKind::Directory;
}

}  // namespace

OptionForm::OptionForm(QWidget *parent) : QWidget(parent), m_tabs(new QTabWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(true);
    layout->addWidget(m_tabs);
}

OptionForm::Tab &OptionForm::createTab(const QString &id, const QString &title, const QString &wiki)
{
    Tab tab;
    tab.title = title;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    tab.content = new QWidget;
    auto *v = new QVBoxLayout(tab.content);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(10);
    v->addStretch(1);
    tab.trailingItems = 1;
    if (!wiki.isEmpty()) {
        auto *link = new QLabel(QStringLiteral("<a href=\"%1%2\">%3</a>")
                                    .arg(kWikiBase, wiki, tr("About these options in the pcb2gcode wiki")));
        link->setOpenExternalLinks(true);
        v->addWidget(link);
        tab.trailingItems++;
    }
    scroll->setWidget(tab.content);
    tab.page = scroll;
    m_tabs->addTab(scroll, title);
    m_tabList.append(tab);
    m_tabIndex.insert(id, m_tabList.size() - 1);
    return m_tabList.last();
}

void OptionForm::addSection(Tab &tab, const QString &title, const QList<const OptionSpec *> &specs)
{
    Section section;
    section.box = new QGroupBox(title);
    auto *v = new QVBoxLayout(section.box);
    section.note = new QLabel(tr("Not used with the current input files and settings."));
    section.note->setEnabled(false);
    section.note->setVisible(false);
    v->addWidget(section.note);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    v->addLayout(form);

    for (const OptionSpec *spec : specs) {
        OptionEditor *e = OptionEditor::create(*spec, m_layout.hint(spec->name), this, section.box);
        if (!e)
            continue;
        form->addRow(e->label(), e->field());
        connect(e, &OptionEditor::changed, this, [this, e] { onEditorChanged(e); });
        connect(e, &OptionEditor::fileChosen, this, &OptionForm::fileChosen);
        m_editors.insert(spec->name, e);
        m_order.append(e);
        m_tabTitleOf.insert(e, tab.title);
        section.editors.append(e);
    }
    auto *v2 = static_cast<QVBoxLayout *>(tab.content->layout());
    v2->insertWidget(v2->count() - tab.trailingItems, section.box);
    tab.sections.append(section);
}

void OptionForm::build(const OptionSchema &schema, const Layout &layout)
{
    m_building = true;
    while (m_tabs->count() > 0) {
        QWidget *page = m_tabs->widget(0);
        m_tabs->removeTab(0);
        delete page;
    }
    m_tabList.clear();
    m_tabIndex.clear();
    m_editors.clear();
    m_order.clear();
    m_tabTitleOf.clear();
    m_schema = schema;
    m_layout = layout;
    m_reported.reset();
    units::setGrammar(schema.grammar.value_or(units::builtInGrammar()));

    QSet<QString> placed;
    for (const LayoutTab &lt : layout.tabs) {
        createTab(lt.id, lt.title, lt.wiki);
        for (const LayoutSection &ls : lt.sections) {
            QList<const OptionSpec *> specs;
            for (const QString &name : ls.options) {
                const OptionSpec *spec = m_schema.find(name);
                if (spec && shownInForm(*spec, layout) && !placed.contains(name)) {
                    specs.append(spec);
                    placed.insert(name);
                }
            }
            if (!specs.isEmpty())
                addSection(m_tabList[m_tabIndex.value(lt.id)], ls.title, specs);
        }
    }

    // Options the layout doesn't know go to the tab of their group, so options added to
    // pcb2gcode show up without changing the GUI.
    QStringList groupOrder;
    for (const OptionGroup &g : m_schema.groups)
        groupOrder.append(g.id);
    for (const OptionSpec &spec : m_schema.options)
        if (!groupOrder.contains(spec.group))
            groupOrder.append(spec.group);
    for (const QString &groupId : groupOrder) {
        QList<const OptionSpec *> specs;
        for (const OptionSpec &spec : m_schema.options)
            if (spec.group == groupId && shownInForm(spec, layout) && !placed.contains(spec.name))
                specs.append(&spec);
        if (specs.isEmpty())
            continue;
        OptionGroup group{groupId, groupId};
        for (const OptionGroup &g : m_schema.groups)
            if (g.id == groupId)
                group = g;
        QString tabId = layout.groupTabs.value(groupId);
        if (tabId.isEmpty() || !m_tabIndex.contains(tabId)) {
            tabId = QStringLiteral("group:") + groupId;
            if (!m_tabIndex.contains(tabId))
                createTab(tabId, shortGroupTitle(group), QString());
        }
        Tab &tab = m_tabList[m_tabIndex.value(tabId)];
        addSection(tab, tab.sections.isEmpty() ? shortGroupTitle(group) : tr("More options"), specs);
        for (const OptionSpec *s : specs)
            placed.insert(s->name);
    }

    for (int i = m_tabList.size() - 1; i >= 0; i--) {
        if (m_tabList.at(i).sections.isEmpty()) {
            QWidget *page = m_tabList.at(i).page;
            m_tabs->removeTab(m_tabs->indexOf(page));
            delete page;
        }
    }

    m_building = false;
    applyUnitSystem();
    refresh();
}

bool OptionForm::supports(const QString &option) const
{
    const OptionSpec *spec = m_schema.find(option);
    return spec && !spec->unsupported;
}

bool OptionForm::isMetric() const
{
    OptionEditor *e = m_editors.value(QStringLiteral("metric"));
    if (!e)
        return true;
    const auto v = e->effectiveValue();
    return v && v->compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

QString OptionForm::browseDirectory(PathKind kind) const
{
    const QString d = m_directories.value(int(kind));
    if (!d.isEmpty())
        return d;
    for (PathKind k : {PathKind::InputGerber, PathKind::InputExcellon, PathKind::Directory}) {
        const QString other = m_directories.value(int(k));
        if (!other.isEmpty())
            return other;
    }
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

void OptionForm::rememberDirectory(PathKind kind, const QString &directory)
{
    m_directories.insert(int(kind), directory);
    if (kind == PathKind::InputGerber || kind == PathKind::InputExcellon) {
        // Drill and Gerber files usually sit together.
        m_directories.insert(int(PathKind::InputGerber), directory);
        m_directories.insert(int(PathKind::InputExcellon), directory);
    }
}

condition::Context OptionForm::conditionContext() const
{
    condition::Context ctx;
    ctx.isOption = [this](const QString &name) { return m_schema.find(name) != nullptr; };
    ctx.value = [this](const QString &name) -> std::optional<QString> {
        if (OptionEditor *e = m_editors.value(name))
            return e->effectiveValue();
        const OptionSpec *spec = m_schema.find(name);
        if (spec && spec->hasDefault)
            return spec->defaultValue;
        return std::nullopt;
    };
    ctx.given = [this](const QString &name) {
        const OptionEditor *e = m_editors.value(name);
        return e && e->ruleEnabled() && !e->isDefault();
    };
    ctx.kind = [this](const QString &name) {
        const OptionSpec *spec = m_schema.find(name);
        if (!spec)
            return condition::Kind::Unknown;
        if (spec->type == OptionType::Bool)
            return condition::Kind::Bool;
        if (spec->isNumeric() && spec->multiplicity == Multiplicity::Single)
            return condition::Kind::Number;
        return condition::Kind::Text;
    };
    ctx.dimension = [this](const QString &name) {
        const OptionSpec *spec = m_schema.find(name);
        return spec ? spec->dimension() : units::Dimension::None;
    };
    ctx.metric = isMetric();
    return ctx;
}

bool OptionForm::isUsed(const OptionEditor *editor) const
{
    return m_schema.holds(editor->spec().usedWhen, conditionContext());
}

QStringList OptionForm::arguments() const
{
    const condition::Context ctx = conditionContext();
    QStringList args;
    for (const OptionEditor *e : m_order) {
        if (!e->ruleEnabled() || e->isDefault() || !m_schema.holds(e->spec().usedWhen, ctx))
            continue;
        for (const QString &v : e->values())
            args.append(QStringLiteral("--%1=%2").arg(e->name(), v));
    }
    return args;
}

QList<OptionForm::Problem> OptionForm::findProblems() const
{
    const condition::Context ctx = conditionContext();
    QList<Problem> out;
    QSet<QString> reportedOptions;
    if (m_reported)
        for (const Problem &p : *m_reported)
            for (const QString &o : p.options)
                reportedOptions.insert(o);
    for (const OptionEditor *e : m_order) {
        if (!e->ruleEnabled() || !m_schema.holds(e->spec().usedWhen, ctx) || reportedOptions.contains(e->name()))
            continue;
        const QString p = e->problem();
        if (!p.isEmpty())
            out.append({QStringLiteral("--%1: %2").arg(e->name(), p), {e->name()}});
    }
    if (m_reported)
        return out + *m_reported;
    // The checks pcb2gcode makes, with its own messages.
    auto value = [&](const QString &name) -> std::optional<QString> {
        const OptionEditor *e = m_editors.value(name);
        if (e && !e->ruleEnabled())
            return std::nullopt;
        return ctx.value(name);
    };
    for (const OptionRule &rule : m_schema.rules) {
        // A rule whose condition this version can't fully read is skipped.
        bool unknown = false;
        if (rule.options.isEmpty() || !m_schema.holds(rule.when, ctx, &unknown) || unknown)
            continue;
        bool failed = false;
        const QString first = rule.options.first();
        const OptionSpec *spec = m_schema.find(first);
        const units::Dimension dim = spec ? spec->dimension() : units::Dimension::None;
        if (rule.kind == "required") {
            const auto v = value(first);
            failed = m_editors.contains(first) && (!v || v->isEmpty());
        } else if (rule.kind == "range") {
            const auto v = value(first);
            if (v && !v->isEmpty() && spec && spec->multiplicity == Multiplicity::Single) {
                if (!rule.minimum.isEmpty()) {
                    const auto c = condition::compareQuantities(*v, rule.minimum, dim, ctx.metric);
                    failed = failed || (c && (*c < 0 || (*c == 0 && rule.minimumExclusive)));
                }
                if (!rule.maximum.isEmpty()) {
                    const auto c = condition::compareQuantities(*v, rule.maximum, dim, ctx.metric);
                    failed = failed || (c && (*c > 0 || (*c == 0 && rule.maximumExclusive)));
                }
            }
        } else if ((rule.kind == "greater" || rule.kind == "greater_or_equal") && rule.options.size() == 2) {
            const auto a = value(first), b = value(rule.options.at(1));
            const bool hasUnless = !rule.unless.isUndefined() && !rule.unless.isNull();
            if (a && b && !(hasUnless && condition::evaluate(rule.unless, ctx, true, &unknown))) {
                const auto c = condition::compareQuantities(*a, *b, dim, ctx.metric);
                failed = c && (rule.kind == "greater" ? *c <= 0 : *c < 0);
            }
        } else if (rule.kind == "forbidden") {
            failed = condition::evaluate(rule.condition, ctx, false, &unknown);
        } else {
            continue;  // a kind of rule this version doesn't know
        }
        if (failed && !unknown)
            out.append({rule.message, rule.options});
    }
    return out;
}

QStringList OptionForm::problems() const
{
    QStringList messages;
    for (const Problem &p : findProblems())
        if (!p.warning && !messages.contains(p.message))
            messages.append(p.message);
    return messages;
}

QStringList OptionForm::warnings() const
{
    QStringList messages;
    for (const Problem &p : findProblems())
        if (p.warning && !messages.contains(p.message))
            messages.append(p.message);
    return messages;
}

void OptionForm::setReportedProblems(const QList<Problem> &problems)
{
    m_reported = problems;
    refresh();
}

void OptionForm::setReportedProblems(const pcb2gcode::CheckResult &result)
{
    QList<Problem> problems;
    for (const pcb2gcode::CheckIssue &e : result.errors)
        problems.append({e.message, e.options, false});
    for (const pcb2gcode::CheckIssue &w : result.warnings) {
        const bool prefixed = w.message.startsWith(QStringLiteral("Warning"), Qt::CaseInsensitive);
        problems.append({prefixed ? w.message : tr("Warning: %1").arg(w.message), w.options, true});
    }
    setReportedProblems(problems);
}

void OptionForm::clearReportedProblems()
{
    if (!m_reported)
        return;
    m_reported.reset();
    refresh();
}

ConfigFile OptionForm::toConfig() const
{
    ConfigFile file;
    for (const OptionEditor *e : m_order) {
        if (e->isDefault())
            continue;
        for (const QString &v : e->values())
            file.entries.append({e->name(), v, !e->ruleEnabled(), m_tabTitleOf.value(e)});
    }
    return file;
}

QStringList OptionForm::applyConfig(const ConfigFile &config, const QString &baseDirectory)
{
    m_building = true;
    QList<ConfigEntry> entries = config.entries;
    QStringList notes = migrateLegacyEntries(entries, m_schema);

    for (OptionEditor *e : m_order)
        e->reset(false);

    bool fileMetric = false;  // pcb2gcode's default
    for (const ConfigEntry &e : entries)
        if (e.key == "metric" && !e.disabled)
            fileMetric = QStringList{"1", "true", "yes", "on"}.contains(e.value.trimmed().toLower());
    if (OptionEditor *m = m_editors.value(QStringLiteral("metric")))
        m->setValues({fileMetric ? QStringLiteral("true") : QStringLiteral("false")}, fileMetric);
    applyUnitSystem();

    QStringList keys;
    QHash<QString, QStringList> active, inactive;
    for (const ConfigEntry &e : entries) {
        if (!keys.contains(e.key))
            keys.append(e.key);
        (e.disabled ? inactive : active)[e.key].append(e.value);
    }

    for (const QString &key : keys) {
        if (key == "metric")
            continue;
        OptionEditor *editor = m_editors.value(key);
        if (!editor) {
            const OptionSpec *spec = m_schema.find(key);
            if (!spec)
                notes << tr("%1 isn't a pcb2gcode option; ignored").arg(key);
            else if (spec->unsupported)
                notes << tr("%1 isn't supported by this pcb2gcode; ignored").arg(key);
            else if (!spec->hasDefault || active.value(key).value(0) != spec->defaultValue)
                notes << tr("%1=%2 is ignored").arg(key, active.value(key).value(0));
            continue;
        }
        QStringList values = active.contains(key) ? active.value(key) : inactive.value(key);
        if (editor->spec().multiplicity == Multiplicity::Single && values.size() > 1)
            values = {values.last()};
        if (isInputPath(editor->spec().path) && !baseDirectory.isEmpty()) {
            for (QString &v : values)
                if (!v.isEmpty() && QFileInfo(v).isRelative())
                    v = QDir::cleanPath(QDir(baseDirectory).absoluteFilePath(v));
        }
        if (editor->spec().dimension() == units::Dimension::Time && active.contains(key)) {
            const auto q = units::parse(values.last(), units::Dimension::Time, fileMetric);
            if (q && q->unit.isEmpty() && q->value != 0)
                notes << tr("%1=%2 has no unit, so pcb2gcode reads it as %2 ms").arg(key, values.last());
        }
        QString error;
        const bool ok = active.contains(key) ? editor->setValues(values, fileMetric, &error)
                                             : editor->setInactiveValues(values, fileMetric, &error);
        if (!ok)
            notes << QStringLiteral("%1: %2").arg(key, error);
    }
    m_building = false;
    refresh();
    emit changed();
    return notes;
}

void OptionForm::resetAll(bool usePresets)
{
    m_building = true;
    if (OptionEditor *m = m_editors.value(QStringLiteral("metric")))
        m->reset(usePresets);
    applyUnitSystem();
    for (OptionEditor *e : m_order)
        if (e->name() != "metric")
            e->reset(usePresets);
    m_building = false;
    refresh();
    emit changed();
}

bool OptionForm::setOptionValues(const QString &name, const QStringList &values, QString *error)
{
    OptionEditor *e = m_editors.value(name);
    if (!e) {
        if (error)
            *error = tr("%1 isn't shown").arg(name);
        return false;
    }
    const bool ok = e->setValues(values, isMetric(), error);
    onEditorChanged(e);
    return ok;
}

void OptionForm::applyUnitSystem()
{
    const bool m = isMetric();
    for (OptionEditor *e : m_order)
        e->unitSystemChanged(m);
}

void OptionForm::onEditorChanged(OptionEditor *editor)
{
    if (m_building)
        return;
    if (editor->name() == "metric")
        applyUnitSystem();
    refresh();
    emit changed();
}

void OptionForm::refresh()
{
    const condition::Context ctx = conditionContext();
    QSet<QString> flagged;
    QHash<QString, QString> why;
    for (const Problem &p : findProblems()) {
        if (p.warning)
            continue;
        for (const QString &o : p.options) {
            flagged.insert(o);
            why[o] += (why.value(o).isEmpty() ? QString() : QStringLiteral("\n")) + p.message;
        }
    }
    for (OptionEditor *e : m_order) {
        e->setUsed(m_schema.holds(e->spec().usedWhen, ctx), m_schema.conditionClause(e->spec().usedWhen));
        e->setRuleEnabled(condition::evaluate(e->hint().enabledIf, ctx));
        e->setHighlighted(!e->isDefault());
        const bool bad = flagged.contains(e->name()) && e->ruleEnabled() && e->isUsed();
        QPalette pal = e->label()->palette();
        pal.setColor(QPalette::WindowText, bad ? QColor(0xc0, 0x20, 0x20) : palette().color(QPalette::WindowText));
        e->label()->setPalette(pal);
        e->label()->setStatusTip(bad ? why.value(e->name()) : QString());
    }
    for (Tab &tab : m_tabList) {
        for (Section &s : tab.sections) {
            const OptionEditor *unused = nullptr;
            bool anyUsed = false;
            for (OptionEditor *e : s.editors) {
                anyUsed = anyUsed || e->isUsed();
                if (!e->isUsed() && !unused)
                    unused = e;
            }
            if (!anyUsed && unused) {
                const QString clause = m_schema.conditionClause(unused->spec().usedWhen);
                s.note->setText(clause.isEmpty() ? tr("Not used with the current input files and settings.")
                                                 : tr("Only used when %1.").arg(clause));
            }
            s.note->setVisible(!anyUsed);
        }
    }
}
