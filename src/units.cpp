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

#include "units.h"

#include <cmath>

namespace units {

namespace {

constexpr double kMmPerInch = 25.4;

class Lexer
{
public:
    explicit Lexer(const QString &text) : text(text) {}

    void skipSpace()
    {
        while (pos < text.size() && text.at(pos).isSpace())
            pos++;
    }

    QString word()
    {
        skipSpace();
        const int start = pos;
        while (pos < text.size() && text.at(pos).isLetter())
            pos++;
        return text.mid(start, pos - start);
    }

    QString number()
    {
        skipSpace();
        const int start = pos;
        while (pos < text.size()) {
            const QChar c = text.at(pos);
            if (!c.isDigit() && c != '-' && c != '+' && c != '.')
                break;
            pos++;
        }
        return text.mid(start, pos - start);
    }

    bool exact(const QString &token)
    {
        skipSpace();
        if (text.mid(pos, token.size()) == token) {
            pos += token.size();
            return true;
        }
        return false;
    }

    bool atEnd()
    {
        skipSpace();
        return pos >= text.size();
    }

    QString rest() const { return text.mid(pos); }

    int pos = 0;

private:
    const QString text;
};

Grammar &activeGrammar()
{
    static Grammar grammar = builtInGrammar();
    return grammar;
}

std::optional<double> lengthUnit(const QString &w)
{
    const auto it = activeGrammar().lengthMm.constFind(w);
    if (it == activeGrammar().lengthMm.constEnd())
        return std::nullopt;
    return *it;
}

std::optional<double> timeUnit(const QString &w)
{
    const auto it = activeGrammar().timeMinutes.constFind(w);
    if (it == activeGrammar().timeMinutes.constEnd())
        return std::nullopt;
    return *it;
}

bool isRevolution(const QString &w)
{
    return activeGrammar().revolutions.contains(w);
}

bool division(Lexer &lex)
{
    return lex.exact("/") || lex.exact("per");
}

}  // namespace

Grammar builtInGrammar()
{
    Grammar g;
    g.lengthMm = {
        {"mm", 1}, {"millimeter", 1}, {"millimeters", 1},
        {"m", 1000}, {"meter", 1000},
        {"in", kMmPerInch}, {"inch", kMmPerInch}, {"inches", kMmPerInch},
        {"thou", kMmPerInch / 1000}, {"thous", kMmPerInch / 1000},
        {"mil", kMmPerInch / 1000}, {"mils", kMmPerInch / 1000},
    };
    g.timeMinutes = {
        {"s", 1.0 / 60}, {"second", 1.0 / 60}, {"seconds", 1.0 / 60},
        {"ms", 1.0 / 60000}, {"millisecond", 1.0 / 60000}, {"milliseconds", 1.0 / 60000},
        {"millis", 1.0 / 60000},
        {"min", 1}, {"mins", 1}, {"minute", 1}, {"minutes", 1},
    };
    g.revolutions = {"rotation", "rotations", "revolutions", "revolution", "rev", "revs", "cycle", "cycles"};
    g.rpm = {"rpm", "RPM"};
    return g;
}

void setGrammar(const Grammar &grammar)
{
    activeGrammar() = grammar;
}

std::optional<Quantity> parse(const QString &text, Dimension dimension, bool metric, QString *error)
{
    auto fail = [&](const QString &why) -> std::optional<Quantity> {
        if (error)
            *error = why;
        return std::nullopt;
    };

    Lexer lex(text);
    const QString numberText = lex.number();
    bool ok = false;
    const double value = numberText.toDouble(&ok);
    if (numberText.isEmpty() || !ok)
        return fail(QStringLiteral("\"%1\" is not a number").arg(text.trimmed()));

    Quantity q;
    q.value = value;

    if (lex.atEnd()) {
        switch (dimension) {
        case Dimension::Length:
            q.canonical = metric ? value : value * kMmPerInch;
            break;
        case Dimension::Velocity:
            q.canonical = metric ? value : value * kMmPerInch;
            break;
        case Dimension::Time:
            q.canonical = value / 1000;
            break;
        case Dimension::Percent:
            q.canonical = value * 100;
            break;
        case Dimension::Rpm:
        case Dimension::None:
            q.canonical = value;
            break;
        }
        return q;
    }

    const int unitStart = lex.pos;
    switch (dimension) {
    case Dimension::None:
        return fail(QStringLiteral("unexpected \"%1\" after the number").arg(lex.rest().trimmed()));
    case Dimension::Length: {
        const auto mm = lengthUnit(lex.word());
        if (!mm)
            return fail(QStringLiteral("unknown length unit in \"%1\"").arg(text.trimmed()));
        q.canonical = value * *mm;
        break;
    }
    case Dimension::Time: {
        const auto minutes = timeUnit(lex.word());
        if (!minutes)
            return fail(QStringLiteral("unknown time unit in \"%1\"").arg(text.trimmed()));
        q.canonical = value * *minutes * 60;
        break;
    }
    case Dimension::Velocity: {
        const auto mm = lengthUnit(lex.word());
        if (!mm || !division(lex))
            return fail(QStringLiteral("\"%1\" is not a speed like 600mm/min").arg(text.trimmed()));
        const auto minutes = timeUnit(lex.word());
        if (!minutes)
            return fail(QStringLiteral("unknown time unit in \"%1\"").arg(text.trimmed()));
        q.canonical = value * *mm / *minutes;
        break;
    }
    case Dimension::Rpm: {
        const int before = lex.pos;
        const QString w = lex.word();
        if (activeGrammar().rpm.contains(w)) {
            q.canonical = value;
            break;
        }
        lex.pos = before;
        if (!isRevolution(lex.word()) || !division(lex))
            return fail(QStringLiteral("\"%1\" is not a spindle speed like 10000rpm").arg(text.trimmed()));
        const auto minutes = timeUnit(lex.word());
        if (!minutes)
            return fail(QStringLiteral("unknown time unit in \"%1\"").arg(text.trimmed()));
        q.canonical = value / *minutes;
        break;
    }
    case Dimension::Percent:
        if (!lex.exact("%"))
            return fail(QStringLiteral("\"%1\" is not a percentage like 50%").arg(text.trimmed()));
        q.canonical = value;
        break;
    }

    if (!lex.atEnd())
        return fail(QStringLiteral("extra characters at the end of \"%1\"").arg(text.trimmed()));
    q.unit = text.mid(unitStart).simplified();
    return q;
}

QString formatNumber(double value, int maxDecimals)
{
    if (std::isinf(value))
        return value > 0 ? QStringLiteral("inf") : QStringLiteral("-inf");
    QString s = QString::number(value, 'f', maxDecimals);
    if (s.contains('.')) {
        while (s.endsWith('0'))
            s.chop(1);
        if (s.endsWith('.'))
            s.chop(1);
    }
    if (s == "-0")
        s = "0";
    return s;
}

bool followsUnitSystem(Dimension dimension)
{
    return dimension == Dimension::Length || dimension == Dimension::Velocity;
}

QString displayUnit(Dimension dimension, bool metric, bool perSecond)
{
    switch (dimension) {
    case Dimension::Length:
        return metric ? QStringLiteral("mm") : QStringLiteral("in");
    case Dimension::Velocity:
        if (perSecond)
            return metric ? QStringLiteral("mm/s") : QStringLiteral("in/s");
        return metric ? QStringLiteral("mm/min") : QStringLiteral("in/min");
    case Dimension::Rpm:
        return QStringLiteral("rpm");
    case Dimension::Time:
        return QStringLiteral("s");
    case Dimension::Percent:
        return QStringLiteral("%");
    case Dimension::None:
        break;
    }
    return QString();
}

static double displayFactor(Dimension dimension, bool metric, bool perSecond)
{
    // canonical = display * factor
    switch (dimension) {
    case Dimension::Length:
        return metric ? 1 : kMmPerInch;
    case Dimension::Velocity:
        return (metric ? 1 : kMmPerInch) * (perSecond ? 60 : 1);
    default:
        return 1;
    }
}

double toDisplay(double canonical, Dimension dimension, bool metric, bool perSecond)
{
    return canonical / displayFactor(dimension, metric, perSecond);
}

double fromDisplay(double display, Dimension dimension, bool metric, bool perSecond)
{
    return display * displayFactor(dimension, metric, perSecond);
}

}  // namespace units
