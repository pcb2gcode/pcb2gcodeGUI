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

#include "optioneditors.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QToolButton>
#include <QWheelEvent>
#include <algorithm>
#include <climits>
#include <cmath>

namespace {

// Spin and combo boxes in a scrolling form must not steal the wheel while scrolling past.
class FormSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        if (hasFocus())
            QSpinBox::wheelEvent(event);
        else
            event->ignore();
    }
};

class FormComboBox : public QComboBox
{
public:
    using QComboBox::QComboBox;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        if (hasFocus())
            QComboBox::wheelEvent(event);
        else
            event->ignore();
    }
};

std::optional<bool> parseBool(const QString &text)
{
    const QString s = text.trimmed().toLower();
    if (s == "true" || s == "1" || s == "yes" || s == "on")
        return true;
    if (s == "false" || s == "0" || s == "no" || s == "off")
        return false;
    return std::nullopt;
}

bool nearlyEqual(double a, double b)
{
    if (std::isinf(a) || std::isinf(b))
        return a == b;
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
}

QString fileFilter(PathKind kind)
{
    const QString all = QObject::tr("All files (*)");
    switch (kind) {
    case PathKind::InputGerber:
        return QObject::tr("Gerber files (*.gbr *.ger *.grb *.gtl *.gbl *.gko *.gm1 *.gml *.gtp *.gbp "
                           "*.gts *.gbs *.gto *.gbo *.g1 *.g2 *.gp1 *.gp2 *.gpt *.gpb *.pho *.art)") +
               ";;" + all;
    case PathKind::InputExcellon:
        return QObject::tr("Excellon drill files (*.drl *.drd *.dri *.xln *.exc *.txt)") + ";;" + all;
    case PathKind::InputGcode:
        return QObject::tr("G-code files (*.nc *.ngc *.gcode *.tap *.cnc *.ncc *.txt)") + ";;" + all;
    case PathKind::InputText:
        return QObject::tr("Text files (*.txt)") + ";;" + all;
    default:
        return all;
    }
}

int decimalsFor(units::Dimension dim, bool metric, bool perSecond, bool fine)
{
    switch (dim) {
    case units::Dimension::Length:
        return metric ? (fine ? 5 : 4) : (fine ? 6 : 5);
    case units::Dimension::Velocity:
        return perSecond ? (metric ? 3 : 4) : (metric ? 2 : 3);
    case units::Dimension::Rpm:
        return 1;
    case units::Dimension::Time:
        return 4;
    case units::Dimension::Percent:
        return 2;
    case units::Dimension::None:
        break;
    }
    return 6;
}

double stepFor(units::Dimension dim, bool metric, bool perSecond, bool fine)
{
    switch (dim) {
    case units::Dimension::Length:
        return metric ? (fine ? 0.01 : 0.1) : (fine ? 0.0005 : 0.005);
    case units::Dimension::Velocity:
        return perSecond ? (metric ? 1 : 0.05) : (metric ? 10 : 0.5);
    case units::Dimension::Rpm:
        return 500;
    case units::Dimension::Time:
        return 0.1;
    case units::Dimension::Percent:
        return 5;
    case units::Dimension::None:
        break;
    }
    return 0.1;
}

// Default bounds in canonical units when pcb2gcode doesn't give one.
double defaultMinimum(units::Dimension dim)
{
    switch (dim) {
    case units::Dimension::Length:
        return -10000;
    case units::Dimension::None:
        return -1e6;
    default:
        return 0;
    }
}

double defaultMaximum(units::Dimension dim)
{
    switch (dim) {
    case units::Dimension::Length:
        return 10000;
    case units::Dimension::Velocity:
        return 1e6;
    case units::Dimension::Rpm:
        return 1e6;
    case units::Dimension::Time:
        return 3600;
    case units::Dimension::Percent:
        return 10000;
    case units::Dimension::None:
        break;
    }
    return 1e6;
}

// ---------------------------------------------------------------------------------------

class BoolEditor : public OptionEditor
{
public:
    BoolEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx, QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent)
    {
        if (hint.choiceLabels.contains("true") && hint.choiceLabels.contains("false")) {
            m_combo = new FormComboBox;
            m_combo->addItem(hint.choiceLabels.value("true"), true);
            m_combo->addItem(hint.choiceLabels.value("false"), false);
            connect(m_combo, &QComboBox::currentIndexChanged, this, [this] { emitChanged(); });
            setEditor(m_combo);
        } else {
            m_check = new QCheckBox;
            connect(m_check, &QCheckBox::toggled, this, [this] { emitChanged(); });
            setEditor(m_check);
            moveLabelOnto(m_check);
        }
    }

protected:
    bool checked() const { return m_combo ? m_combo->currentData().toBool() : m_check->isChecked(); }

    void setChecked(bool on)
    {
        if (m_combo)
            m_combo->setCurrentIndex(on ? 0 : 1);
        else
            m_check->setChecked(on);
    }

    QStringList editorValues() const override
    {
        if (!m_spec.hasDefault && !checked())
            return {};
        return {checked() ? QStringLiteral("true") : QStringLiteral("false")};
    }

    bool loadValues(const QStringList &values, bool, QString *error) override
    {
        if (values.isEmpty()) {
            setChecked(false);
            return true;
        }
        const auto b = parseBool(values.last().isEmpty() && m_spec.hasImplicit ? m_spec.implicitValue
                                                                               : values.last());
        if (!b) {
            if (error)
                *error = tr("\"%1\" is not true or false").arg(values.last());
            return false;
        }
        setChecked(*b);
        return true;
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        return parseBool(values.last()) == parseBool(m_spec.defaultValue);
    }

private:
    QCheckBox *m_check = nullptr;
    QComboBox *m_combo = nullptr;
};

class ChoiceEditor : public OptionEditor
{
public:
    ChoiceEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx, QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent)
    {
        m_combo = new FormComboBox;
        if (!spec.hasDefault && spec.requiredWhen.isEmpty())
            m_combo->addItem(tr("(not set)"), QString());
        for (const OptionChoice &c : spec.choices)
            m_combo->addItem(hint.choiceLabels.value(c.value, c.value), c.value);
        connect(m_combo, &QComboBox::currentIndexChanged, this, [this] { emitChanged(); });
        setEditor(m_combo);
    }

protected:
    QStringList editorValues() const override
    {
        const QString v = m_combo->currentData().toString();
        return v.isEmpty() ? QStringList() : QStringList{v};
    }

    bool loadValues(const QStringList &values, bool, QString *error) override
    {
        if (values.isEmpty()) {
            m_combo->setCurrentIndex(0);
            return true;
        }
        const QString canonical = m_spec.canonicalChoice(values.last());
        const int index = m_combo->findData(canonical);
        if (canonical.isEmpty() || index < 0) {
            if (error)
                *error = tr("\"%1\" is not one of the choices").arg(values.last());
            return false;
        }
        m_combo->setCurrentIndex(index);
        return true;
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        return m_spec.canonicalChoice(values.last()) == m_spec.canonicalChoice(m_spec.defaultValue);
    }

private:
    QComboBox *m_combo;
};

class IntegerEditor : public OptionEditor
{
public:
    IntegerEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx, QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent)
    {
        m_spin = new FormSpinBox;
        int minimum = spec.type == OptionType::Int ? INT_MIN : 0;
        int maximum = INT_MAX;
        bool ok = false;
        const int specMin = spec.minimum.toInt(&ok);
        if (ok)
            minimum = std::max(minimum, specMin);
        const int specMax = spec.maximum.toInt(&ok);
        if (ok)
            maximum = std::min(maximum, specMax);
        m_spin->setRange(minimum, maximum);
        m_spin->setMinimumWidth(110);
        connect(m_spin, &QSpinBox::valueChanged, this, [this] { emitChanged(); });
        setEditor(m_spin);
    }

protected:
    QStringList editorValues() const override { return {QString::number(m_spin->value())}; }

    bool loadValues(const QStringList &values, bool, QString *error) override
    {
        if (values.isEmpty()) {
            m_spin->setValue(std::max(0, m_spin->minimum()));
            return true;
        }
        bool ok = false;
        const qlonglong v = values.last().trimmed().toLongLong(&ok);
        if (!ok) {
            if (error)
                *error = tr("\"%1\" is not a whole number").arg(values.last());
            return false;
        }
        m_spin->setValue(int(std::clamp<qlonglong>(v, m_spin->minimum(), m_spin->maximum())));
        if (v < m_spin->minimum() || v > m_spin->maximum()) {
            if (error)
                *error = tr("%1 is out of range; using %2").arg(v).arg(m_spin->value());
            return false;
        }
        return true;
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        return values.last().trimmed().toLongLong() == m_spec.defaultValue.trimmed().toLongLong();
    }

private:
    QSpinBox *m_spin;
};

class QuantityEditor : public OptionEditor
{
public:
    QuantityEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx, QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent), m_dim(spec.dimension()),
          m_bare(spec.type == OptionType::Double)
    {
        m_spin = new QuantitySpinBox;
        m_spin->setMinimumWidth(140);
        connect(m_spin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
            m_canonical = units::fromDisplay(v, m_dim, m_metric, m_hint.perSecond);
            emitChanged();
        });
        setEditor(m_spin);
        m_metric = metric();
        configure();
    }

protected:
    QString unit() const { return units::displayUnit(m_dim, m_metric, m_hint.perSecond); }

    std::optional<double> bound(const QString &text) const
    {
        if (text.isEmpty())
            return std::nullopt;
        const auto q = units::parse(text, m_dim, m_metric);
        if (!q)
            return std::nullopt;
        return q->canonical;
    }

    double minimum() const
    {
        if (const auto b = bound(m_spec.minimum))
            return *b;
        return bound(m_hint.minimum).value_or(defaultMinimum(m_dim));
    }

    void configure()
    {
        const QSignalBlocker blocker(m_spin);
        const QString u = unit();
        m_spin->setSuffix(u.isEmpty() ? QString() : " " + u);
        m_spin->setDecimals(decimalsFor(m_dim, m_metric, m_hint.perSecond, m_hint.fine));
        m_spin->setSingleStep(stepFor(m_dim, m_metric, m_hint.perSecond, m_hint.fine));
        const double min = minimum();
        const double max = bound(m_spec.maximum).value_or(defaultMaximum(m_dim));
        m_spin->setRange(units::toDisplay(min, m_dim, m_metric, m_hint.perSecond),
                         units::toDisplay(max, m_dim, m_metric, m_hint.perSecond));
        m_spin->setValue(units::toDisplay(m_canonical, m_dim, m_metric, m_hint.perSecond));
    }

    QStringList editorValues() const override
    {
        const QString number = units::formatNumber(m_spin->value(), m_spin->decimals());
        return {m_bare ? number : number + unit()};
    }

    bool loadValues(const QStringList &values, bool sourceMetric, QString *error) override
    {
        if (values.isEmpty()) {
            m_canonical = std::clamp(0.0, minimum(), bound(m_spec.maximum).value_or(defaultMaximum(m_dim)));
            configure();
            return true;
        }
        QString why;
        const auto q = units::parse(values.last(), m_dim, sourceMetric, &why);
        if (!q || !std::isfinite(q->canonical)) {
            if (error)
                *error = q ? tr("\"%1\" can't be set explicitly").arg(values.last()) : why;
            return false;
        }
        const double min = minimum();
        const double max = bound(m_spec.maximum).value_or(defaultMaximum(m_dim));
        m_canonical = std::clamp(q->canonical, min, max);
        configure();
        if (m_canonical != q->canonical) {
            if (error)
                *error = tr("%1 is out of range; using %2%3")
                             .arg(values.last(), units::formatNumber(m_spin->value()), unit());
            return false;
        }
        return true;
    }

    void applyUnits(bool metric) override
    {
        if (!units::followsUnitSystem(m_dim))
            return;
        m_metric = metric;
        configure();
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        const auto a = units::parse(values.last(), m_dim, m_metric);
        const auto b = units::parse(m_spec.defaultValue, m_dim, m_metric);
        return a && b && nearlyEqual(a->canonical, b->canonical);
    }

private:
    QuantitySpinBox *m_spin;
    units::Dimension m_dim;
    bool m_bare;
    bool m_metric = true;
    double m_canonical = 0;
};

// --milling-overlap: a percentage of the tool diameter or a length.
class LengthOrPercentEditor : public OptionEditor
{
public:
    LengthOrPercentEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx,
                          QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent)
    {
        auto *box = new QWidget;
        auto *layout = new QHBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        m_spin = new QuantitySpinBox;
        m_spin->setMinimumWidth(110);
        m_unit = new FormComboBox;
        m_unit->addItem(QStringLiteral("%"), true);
        m_unit->addItem(QString(), false);
        layout->addWidget(m_spin, 1);
        layout->addWidget(m_unit);
        connect(m_spin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
            if (m_percent)
                m_value = v;
            else
                m_value = units::fromDisplay(v, units::Dimension::Length, m_metric);
            emitChanged();
        });
        connect(m_unit, &QComboBox::currentIndexChanged, this, [this] {
            const bool percent = m_unit->currentData().toBool();
            if (percent == m_percent)
                return;
            m_percent = percent;
            m_value = percent ? m_spin->value()
                              : units::fromDisplay(m_spin->value(), units::Dimension::Length, m_metric);
            configure();
            emitChanged();
        });
        setEditor(box);
        m_metric = metric();
        configure();
    }

protected:
    void configure()
    {
        const QSignalBlocker b1(m_spin), b2(m_unit);
        m_unit->setItemText(1, units::displayUnit(units::Dimension::Length, m_metric));
        m_unit->setCurrentIndex(m_percent ? 0 : 1);
        if (m_percent) {
            m_spin->setDecimals(2);
            m_spin->setSingleStep(5);
            m_spin->setRange(0, 100);
            m_spin->setValue(m_value);
        } else {
            m_spin->setDecimals(decimalsFor(units::Dimension::Length, m_metric, false, true));
            m_spin->setSingleStep(stepFor(units::Dimension::Length, m_metric, false, true));
            m_spin->setRange(0, units::toDisplay(100, units::Dimension::Length, m_metric));
            m_spin->setValue(units::toDisplay(m_value, units::Dimension::Length, m_metric));
        }
    }

    QStringList editorValues() const override
    {
        const QString n = units::formatNumber(m_spin->value(), m_spin->decimals());
        return {m_percent ? n + "%" : n + units::displayUnit(units::Dimension::Length, m_metric)};
    }

    // Like pcb2gcode: try a length first, then a percentage.
    static std::optional<std::pair<bool, double>> parseValue(const QString &text, bool metric)
    {
        if (const auto l = units::parse(text, units::Dimension::Length, metric))
            return std::make_pair(false, l->canonical);
        if (const auto p = units::parse(text, units::Dimension::Percent, metric); p && !p->unit.isEmpty())
            return std::make_pair(true, p->canonical);
        return std::nullopt;
    }

    bool loadValues(const QStringList &values, bool sourceMetric, QString *error) override
    {
        if (values.isEmpty()) {
            m_percent = true;
            m_value = 50;
            configure();
            return true;
        }
        const auto v = parseValue(values.last(), sourceMetric);
        if (!v) {
            if (error)
                *error = tr("\"%1\" is neither a percentage nor a length").arg(values.last());
            return false;
        }
        m_percent = v->first;
        m_value = v->second;
        configure();
        return true;
    }

    void applyUnits(bool metric) override
    {
        m_metric = metric;
        configure();
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        const auto a = parseValue(values.last(), m_metric);
        const auto b = parseValue(m_spec.defaultValue, m_metric);
        return a && b && a->first == b->first && nearlyEqual(a->second, b->second);
    }

private:
    QuantitySpinBox *m_spin;
    QComboBox *m_unit;
    bool m_percent = true;
    double m_value = 50;  // percent, or mm
    bool m_metric = true;
};

// Comma-separated lists: --mill-diameters=0.2mm,0.5mm, --drills-available=0.8mm,1mm:-0.1mm:+0.1mm
class ListEditor : public OptionEditor
{
public:
    ListEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx, QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent)
    {
        m_edit = new QLineEdit;
        m_edit->setPlaceholderText(!hint.placeholder.isEmpty() ? hint.placeholder : spec.defaultNote);
        m_edit->setClearButtonEnabled(true);
        connect(m_edit, &QLineEdit::textChanged, this, [this] { emitChanged(); });
        connect(m_edit, &QLineEdit::editingFinished, this, [this] { normalizeText(m_metric); });
        setEditor(m_edit);
        m_metric = metric();
    }

protected:
    bool hasUnits() const { return m_spec.dimension() != units::Dimension::None; }

    // Gives every bare number in `text` the unit it was typed in.
    std::optional<QStringList> normalized(const QString &text, bool sourceMetric, QString *error) const
    {
        QStringList out;
        if (text.trimmed().isEmpty())
            return out;
        const units::Dimension dim = m_spec.dimension();
        const QString unit = units::displayUnit(dim, sourceMetric);
        for (const QString &rawPart : text.split(',')) {
            const QString part = rawPart.trimmed();
            if (part.isEmpty())
                continue;
            if (!hasUnits()) {
                out.append(part);
                continue;
            }
            QStringList pieces = m_spec.type == OptionType::Drill ? part.split(':') : QStringList{part};
            if (pieces.size() > 3) {
                if (error)
                    *error = tr("\"%1\" has too many parts").arg(part);
                return std::nullopt;
            }
            for (QString &piece : pieces) {
                piece = piece.trimmed();
                QString why;
                const auto q = units::parse(piece, dim, sourceMetric, &why);
                if (!q) {
                    if (error)
                        *error = why;
                    return std::nullopt;
                }
                if (q->unit.isEmpty())
                    piece += unit;
            }
            out.append(pieces.join(':'));
        }
        return out;
    }

    void normalizeText(bool sourceMetric)
    {
        const auto n = normalized(m_edit->text(), sourceMetric, nullptr);
        if (n && hasUnits()) {
            const QString text = n->join(", ");
            if (text != m_edit->text()) {
                const QSignalBlocker blocker(m_edit);
                m_edit->setText(text);
            }
        }
    }

    QStringList editorValues() const override
    {
        const auto n = normalized(m_edit->text(), m_metric, nullptr);
        if (!n)
            return {m_edit->text().trimmed()};
        if (n->isEmpty())
            return {};
        return {n->join(',')};
    }

    bool loadValues(const QStringList &values, bool sourceMetric, QString *error) override
    {
        const QString text = values.join(',');
        const auto n = normalized(text, sourceMetric, error);
        const QSignalBlocker blocker(m_edit);
        m_edit->setText(n ? n->join(", ") : text);
        return bool(n);
    }

    void applyUnits(bool metric) override
    {
        normalizeText(m_metric);
        m_metric = metric;
    }

    QString editorProblem() const override
    {
        QString why;
        if (!normalized(m_edit->text(), m_metric, &why))
            return why;
        return QString();
    }

    QList<double> canonicalList(const QString &text) const
    {
        QList<double> out;
        const auto n = normalized(text, m_metric, nullptr);
        if (!n)
            return out;
        for (const QString &part : *n)
            for (const QString &piece : part.split(':'))
                if (const auto q = units::parse(piece, m_spec.dimension(), m_metric))
                    out.append(q->canonical);
        return out;
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        if (!hasUnits())
            return values.join(',') == m_spec.defaultValue;
        const QList<double> a = canonicalList(values.join(','));
        const QList<double> b = canonicalList(m_spec.defaultValue);
        if (a.size() != b.size())
            return false;
        for (int i = 0; i < a.size(); i++)
            if (!nearlyEqual(a.at(i), b.at(i)))
                return false;
        return true;
    }

private:
    QLineEdit *m_edit;
    bool m_metric = true;
};

class TextEditor : public OptionEditor
{
public:
    TextEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx, QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent)
    {
        m_edit = new QLineEdit;
        QString placeholder = hint.placeholder;
        if (placeholder.isEmpty() && spec.hasDefault && !spec.defaultValue.isEmpty())
            placeholder = tr("default: %1").arg(spec.defaultValue);
        if (placeholder.isEmpty())
            placeholder = spec.defaultNote;
        m_edit->setPlaceholderText(placeholder);
        connect(m_edit, &QLineEdit::textChanged, this, [this] { emitChanged(); });

        QToolButton *browse = nullptr;
        if (spec.path != PathKind::None && spec.path != PathKind::OutputFile) {
            m_edit->setClearButtonEnabled(true);
            browse = new QToolButton;
            browse->setText(QStringLiteral("…"));
            browse->setToolTip(spec.path == PathKind::Directory ? tr("Choose a folder") : tr("Choose a file"));
            connect(browse, &QToolButton::clicked, this, &TextEditor::browse);
        }
        setEditor(m_edit, browse);
    }

protected:
    void browse()
    {
        const QString start = m_edit->text().isEmpty() ? m_context->browseDirectory(m_spec.path)
                                                       : QFileInfo(m_edit->text()).absolutePath();
        const QString title = tr("Select %1").arg(m_hint.label.isEmpty() ? m_spec.name : m_hint.label.toLower());
        QString chosen;
        if (m_spec.path == PathKind::Directory)
            chosen = QFileDialog::getExistingDirectory(m_edit, title, start);
        else
            chosen = QFileDialog::getOpenFileName(m_edit, title, start, fileFilter(m_spec.path));
        if (chosen.isEmpty())
            return;
        m_context->rememberDirectory(m_spec.path, m_spec.path == PathKind::Directory
                                                      ? chosen
                                                      : QFileInfo(chosen).absolutePath());
        m_edit->setText(chosen);
        emit fileChosen();
    }

    QStringList editorValues() const override
    {
        const QString t = m_edit->text();
        return t.trimmed().isEmpty() ? QStringList() : QStringList{t};
    }

    bool loadValues(const QStringList &values, bool, QString *) override
    {
        const QSignalBlocker blocker(m_edit);
        m_edit->setText(values.join(','));
        return true;
    }

    QString editorProblem() const override
    {
        const QString t = m_edit->text().trimmed();
        if (t.isEmpty())
            return QString();
        if (m_spec.path == PathKind::Directory && !QFileInfo(t).isDir())
            return tr("folder not found");
        if (m_spec.path != PathKind::None && m_spec.path != PathKind::OutputFile &&
            m_spec.path != PathKind::Directory && !QFileInfo(t).isFile())
            return tr("file not found");
        return QString();
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        return values.join(',') == m_spec.defaultValue;
    }

private:
    QLineEdit *m_edit;
};

// Options given once per line, such as --pre-milling-gcode.
class LinesEditor : public OptionEditor
{
public:
    LinesEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *ctx, QWidget *parent)
        : OptionEditor(spec, hint, ctx, parent)
    {
        m_edit = new QPlainTextEdit;
        m_edit->setTabChangesFocus(true);
        m_edit->setPlaceholderText(hint.placeholder.isEmpty() ? tr("One line of G-code per line")
                                                              : hint.placeholder);
        const QFontMetrics fm(m_edit->font());
        m_edit->setFixedHeight(fm.lineSpacing() * 3 + 12);
        connect(m_edit, &QPlainTextEdit::textChanged, this, [this] { emitChanged(); });
        setEditor(m_edit);
    }

protected:
    QStringList editorValues() const override
    {
        QStringList lines;
        for (const QString &line : m_edit->toPlainText().split('\n'))
            if (!line.trimmed().isEmpty())
                lines.append(line.trimmed());
        return lines;
    }

    bool loadValues(const QStringList &values, bool, QString *) override
    {
        const QSignalBlocker blocker(m_edit);
        m_edit->setPlainText(values.join('\n'));
        return true;
    }

    bool sameAsDefault(const QStringList &values) const override
    {
        return values.join('\n') == m_spec.defaultValue;
    }

private:
    QPlainTextEdit *m_edit;
};

}  // namespace

// ---------------------------------------------------------------------------------------

QString QuantitySpinBox::textFromValue(double value) const
{
    QLocale l = locale();
    l.setNumberOptions(QLocale::OmitGroupSeparator);
    QString s = l.toString(value, 'f', decimals());
    const QString point = l.decimalPoint();
    if (s.contains(point)) {
        while (s.endsWith('0'))
            s.chop(1);
        if (s.endsWith(point))
            s.chop(point.size());
    }
    if (s == "-0")
        s = "0";
    return s;
}

QString humanizeOptionName(const QString &name)
{
    QString s = name;
    s.replace('-', ' ');
    if (!s.isEmpty())
        s[0] = s.at(0).toUpper();
    return s;
}

OptionEditor *OptionEditor::create(const OptionSpec &spec, const OptionHint &hint, EditorContext *context,
                                   QWidget *parent)
{
    OptionEditor *editor = nullptr;
    switch (spec.type) {
    case OptionType::None:
        return nullptr;
    case OptionType::Bool:
        editor = new BoolEditor(spec, hint, context, parent);
        break;
    case OptionType::Enum:
        if (spec.choices.isEmpty())
            editor = new TextEditor(spec, hint, context, parent);
        else
            editor = new ChoiceEditor(spec, hint, context, parent);
        break;
    case OptionType::Int:
    case OptionType::UInt:
        if (spec.multiplicity == Multiplicity::Single)
            editor = new IntegerEditor(spec, hint, context, parent);
        else
            editor = new ListEditor(spec, hint, context, parent);
        break;
    case OptionType::Double:
    case OptionType::Length:
    case OptionType::Velocity:
    case OptionType::Rpm:
    case OptionType::Time:
    case OptionType::Percent:
        if (spec.multiplicity == Multiplicity::Single)
            editor = new QuantityEditor(spec, hint, context, parent);
        else
            editor = new ListEditor(spec, hint, context, parent);
        break;
    case OptionType::LengthOrPercent:
        editor = new LengthOrPercentEditor(spec, hint, context, parent);
        break;
    case OptionType::Drill:
        editor = new ListEditor(spec, hint, context, parent);
        break;
    case OptionType::String:
        if (spec.multiplicity == Multiplicity::Repeat)
            editor = new LinesEditor(spec, hint, context, parent);
        else if (spec.multiplicity == Multiplicity::CommaList)
            editor = new ListEditor(spec, hint, context, parent);
        else
            editor = new TextEditor(spec, hint, context, parent);
        break;
    }
    editor->reset(true);
    return editor;
}

OptionEditor::OptionEditor(const OptionSpec &spec, const OptionHint &hint, EditorContext *context,
                           QWidget *parent)
    : QObject(parent), m_spec(spec), m_hint(hint), m_context(context)
{
    m_label = new QLabel(hint.label.isEmpty() ? humanizeOptionName(spec.name) : hint.label, parent);
    m_textWidget = m_label;
    m_label->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_label, &QLabel::customContextMenuRequested, this,
            [this](const QPoint &pos) { showResetMenu(m_label->mapToGlobal(pos)); });

    m_field = new QWidget(parent);
    m_fieldLayout = new QHBoxLayout(m_field);
    m_fieldLayout->setContentsMargins(0, 0, 0, 0);

    const bool numeric = spec.isNumeric() || spec.type == OptionType::LengthOrPercent;
    if (numeric && spec.multiplicity == Multiplicity::Single &&
        (!spec.hasDefault || spec.defaultIsInfinite()) && spec.requiredWhen.isEmpty()) {
        m_override = new QCheckBox;
        m_override->setToolTip(tr("Tick to set a value; otherwise pcb2gcode decides"));
        connect(m_override, &QCheckBox::toggled, this, [this] {
            updateEnabled();
            emitChanged();
        });
        m_fieldLayout->addWidget(m_override);
        QString note = spec.defaultNote;
        if (note.startsWith("inf:"))
            note = note.mid(4).trimmed();
        if (!note.isEmpty()) {
            m_note = new QLabel(tr("default: %1").arg(note));
            m_note->setEnabled(false);
        }
    }
    updateTooltips();
}

void OptionEditor::setEditor(QWidget *editor, QWidget *extra)
{
    m_editor = editor;
    m_label->setBuddy(editor);
    m_fieldLayout->addWidget(editor, 1);
    if (extra)
        m_fieldLayout->addWidget(extra);
    if (m_note)
        m_fieldLayout->addWidget(m_note, 1);
    updateEnabled();
    updateTooltips();
}

void OptionEditor::moveLabelOnto(QAbstractButton *button)
{
    button->setText(m_label->text());
    m_label->clear();
    m_textWidget = button;
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(button, &QWidget::customContextMenuRequested, this,
            [this, button](const QPoint &pos) { showResetMenu(button->mapToGlobal(pos)); });
    updateTooltips();
}

void OptionEditor::showResetMenu(const QPoint &globalPos)
{
    QMenu menu;
    menu.addAction(tr("Reset to pcb2gcode's default"), this, [this] {
        reset(false);
        emitChanged();
    });
    menu.exec(globalPos);
}

void OptionEditor::updateTooltips()
{
    QString tip = QStringLiteral("<p><b>--%1</b></p>").arg(m_spec.name.toHtmlEscaped());
    if (!m_spec.description.isEmpty())
        tip += QStringLiteral("<p>%1</p>").arg(m_spec.description.toHtmlEscaped());
    if (m_spec.hasDefault && !m_spec.defaultValue.isEmpty())
        tip += QStringLiteral("<p>%1 <code>%2</code>%3</p>")
                   .arg(tr("pcb2gcode's default:"), m_spec.defaultValue.toHtmlEscaped(),
                        m_spec.defaultNote.isEmpty() ? QString() : " (" + m_spec.defaultNote.toHtmlEscaped() + ")");
    else if (!m_spec.defaultNote.isEmpty())
        tip += QStringLiteral("<p>%1 %2</p>").arg(tr("If not set:"), m_spec.defaultNote.toHtmlEscaped());
    if (!m_spec.requiredWhen.isEmpty())
        tip += QStringLiteral("<p>%1</p>").arg(tr("Required"));
    if (!m_used)
        tip += QStringLiteral("<p><i>%1</i></p>").arg(tr("Not used with the current files and settings."));
    m_label->setToolTip(tip);
    m_textWidget->setToolTip(tip);
    if (m_editor)
        m_editor->setToolTip(tip);
}

void OptionEditor::updateEnabled()
{
    m_field->setEnabled(m_ruleEnabled);
    if (m_editor)
        m_editor->setEnabled(!m_override || m_override->isChecked());
    if (m_note)
        m_note->setVisible(!m_override->isChecked());
}

void OptionEditor::emitChanged()
{
    if (!m_loading)
        emit changed();
}

QStringList OptionEditor::values() const
{
    if (m_override && !m_override->isChecked())
        return {};
    return editorValues();
}

std::optional<QString> OptionEditor::effectiveValue() const
{
    const QStringList v = values();
    if (!v.isEmpty())
        return v.join(m_spec.multiplicity == Multiplicity::Repeat ? QStringLiteral("\n") : QStringLiteral(","));
    if (m_spec.hasDefault)
        return m_spec.defaultValue;
    return std::nullopt;
}

bool OptionEditor::sameAsDefault(const QStringList &values) const
{
    return values.join(',').trimmed().compare(m_spec.defaultValue.trimmed(), Qt::CaseInsensitive) == 0;
}

bool OptionEditor::isDefault() const
{
    const QStringList v = values();
    if (v.isEmpty())
        return true;
    if (!m_spec.hasDefault || m_spec.defaultIsInfinite())
        return false;
    return sameAsDefault(v);
}

QString OptionEditor::problem() const
{
    if (!m_ruleEnabled || (m_override && !m_override->isChecked()))
        return QString();
    return editorProblem();
}

bool OptionEditor::setValues(const QStringList &values, bool metric, QString *error)
{
    m_loading = true;
    const bool ok = loadValues(values, metric, error);
    if (m_override)
        m_override->setChecked(!values.isEmpty());
    m_loading = false;
    updateEnabled();
    return ok;
}

bool OptionEditor::setInactiveValues(const QStringList &values, bool metric, QString *error)
{
    if (!m_override)
        return setValues(values, metric, error);
    m_loading = true;
    const bool ok = loadValues(values, metric, error);
    m_override->setChecked(false);
    m_loading = false;
    updateEnabled();
    return ok;
}

QString OptionEditor::startValue(bool usePreset) const
{
    if (usePreset && !m_hint.preset.isEmpty())
        return m_hint.preset;
    if (m_spec.hasDefault && !m_spec.defaultIsInfinite())
        return m_spec.defaultValue;
    if (!m_spec.example.isEmpty())
        return m_spec.example;
    if (!m_hint.preset.isEmpty())
        return m_hint.preset;
    return QString();
}

void OptionEditor::reset(bool usePreset)
{
    m_loading = true;
    const QString start = startValue(usePreset);
    if (start.isEmpty() || !loadValues({start}, metric(), nullptr))
        loadValues({}, metric(), nullptr);
    if (m_override)
        m_override->setChecked(false);
    m_loading = false;
    updateEnabled();
}

void OptionEditor::unitSystemChanged(bool metric)
{
    m_loading = true;
    applyUnits(metric);
    m_loading = false;
}

bool OptionEditor::isOverridden() const
{
    return m_override && m_override->isChecked();
}

void OptionEditor::setOverridden(bool on)
{
    if (m_override)
        m_override->setChecked(on);
}

void OptionEditor::setRuleEnabled(bool enabled)
{
    if (m_ruleEnabled == enabled)
        return;
    m_ruleEnabled = enabled;
    updateEnabled();
}

void OptionEditor::setUsed(bool used, const QString &)
{
    if (m_used == used)
        return;
    m_used = used;
    updateTooltips();
}

void OptionEditor::setHighlighted(bool changed)
{
    QFont f = m_textWidget->font();
    if (f.bold() != changed) {
        f.setBold(changed);
        m_textWidget->setFont(f);
    }
}
