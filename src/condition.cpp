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

#include "condition.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace condition {

namespace {

struct Token {
    enum Type { Word, Number, String, Op, LParen, RParen, End } type;
    QString text;
};

QList<Token> tokenize(const QString &s)
{
    QList<Token> tokens;
    int i = 0;
    while (i < s.size()) {
        const QChar c = s.at(i);
        if (c.isSpace()) {
            i++;
        } else if (c == '(') {
            tokens.append({Token::LParen, "("});
            i++;
        } else if (c == ')') {
            tokens.append({Token::RParen, ")"});
            i++;
        } else if (c == '\'' || c == '"') {
            const int end = s.indexOf(c, i + 1);
            if (end < 0)
                throw std::runtime_error("unterminated string");
            tokens.append({Token::String, s.mid(i + 1, end - i - 1)});
            i = end + 1;
        } else if (c.isDigit() || ((c == '-' || c == '+' || c == '.') && i + 1 < s.size() &&
                                   (s.at(i + 1).isDigit() || s.at(i + 1) == '.'))) {
            const int start = i++;
            while (i < s.size() && (s.at(i).isDigit() || s.at(i) == '.'))
                i++;
            while (i < s.size() && (s.at(i).isLetter() || s.at(i) == '%' || s.at(i) == '/'))
                i++;
            tokens.append({Token::Number, s.mid(start, i - start)});
        } else if (c.isLetter()) {
            const int start = i;
            while (i < s.size() && (s.at(i).isLetterOrNumber() || s.at(i) == '-' || s.at(i) == '_'))
                i++;
            tokens.append({Token::Word, s.mid(start, i - start)});
        } else {
            static const QStringList ops = {"&&", "||", "==", "!=", "<=", ">=", "<", ">", "!"};
            bool matched = false;
            for (const QString &op : ops) {
                if (s.mid(i, op.size()) == op) {
                    tokens.append({Token::Op, op});
                    i += op.size();
                    matched = true;
                    break;
                }
            }
            if (!matched)
                throw std::runtime_error(QStringLiteral("unexpected '%1'").arg(c).toStdString());
        }
    }
    tokens.append({Token::End, QString()});
    return tokens;
}

// An operand: either an option or a literal.
struct Operand {
    bool isOption = false;
    QString text;  // option name or literal text
    bool quoted = false;
};

class Parser
{
public:
    Parser(const QList<Token> &tokens, const Context &context) : tokens(tokens), ctx(context) {}

    bool parse()
    {
        const bool v = parseOr();
        if (peek().type != Token::End)
            throw std::runtime_error("unexpected '" + peek().text.toStdString() + "'");
        return v;
    }

private:
    const Token &peek() const { return tokens.at(pos); }
    Token take() { return tokens.at(pos++); }

    bool acceptOp(const QString &op)
    {
        if (peek().type == Token::Op && peek().text == op) {
            pos++;
            return true;
        }
        return false;
    }

    bool parseOr()
    {
        bool v = parseAnd();
        while (acceptOp("||")) {
            const bool rhs = parseAnd();
            v = v || rhs;
        }
        return v;
    }

    bool parseAnd()
    {
        bool v = parseNot();
        while (acceptOp("&&")) {
            const bool rhs = parseNot();
            v = v && rhs;
        }
        return v;
    }

    bool parseNot()
    {
        if (acceptOp("!"))
            return !parseNot();
        return parseComparison();
    }

    bool parseComparison()
    {
        if (peek().type == Token::LParen) {
            take();
            const bool v = parseOr();
            if (take().type != Token::RParen)
                throw std::runtime_error("missing ')'");
            return v;
        }
        const Operand lhs = parseOperand();
        static const QStringList cmps = {"==", "!=", "<", "<=", ">", ">="};
        if (peek().type == Token::Op && cmps.contains(peek().text)) {
            const QString op = take().text;
            const Operand rhs = parseOperand();
            return compare(lhs, op, rhs);
        }
        return truthy(lhs);
    }

    Operand parseOperand()
    {
        const Token t = take();
        switch (t.type) {
        case Token::Word:
            if (ctx.isOption && ctx.isOption(t.text))
                return {true, t.text, false};
            return {false, t.text, false};
        case Token::Number:
            return {false, t.text, false};
        case Token::String:
            return {false, t.text, true};
        default:
            throw std::runtime_error("expected a value");
        }
    }

    Kind kindOf(const Operand &o) const
    {
        if (o.isOption)
            return ctx.kind ? ctx.kind(o.text) : Kind::Unknown;
        return Kind::Unknown;
    }

    static std::optional<bool> boolValue(const QString &v)
    {
        const QString s = v.trimmed().toLower();
        if (s == "true" || s == "1" || s == "yes" || s == "on")
            return true;
        if (s == "false" || s == "0" || s == "no" || s == "off")
            return false;
        return std::nullopt;
    }

    std::optional<double> number(const Operand &o, units::Dimension dim) const
    {
        QString text;
        if (o.isOption) {
            const auto v = ctx.value(o.text);
            if (!v)
                return std::nullopt;
            text = *v;
        } else {
            text = o.text;
        }
        if (text.trimmed().compare("inf", Qt::CaseInsensitive) == 0)
            return std::numeric_limits<double>::infinity();
        if (text.trimmed().compare("-inf", Qt::CaseInsensitive) == 0)
            return -std::numeric_limits<double>::infinity();
        const auto q = units::parse(text, dim, ctx.metric);
        if (!q)
            return std::nullopt;
        return q->canonical;
    }

    std::optional<QString> textOf(const Operand &o) const
    {
        if (o.isOption)
            return ctx.value(o.text);
        return o.text;
    }

    bool truthy(const Operand &o) const
    {
        if (!o.isOption) {
            if (o.quoted)
                return !o.text.isEmpty();
            const auto b = boolValue(o.text);
            return b.value_or(!o.text.isEmpty());
        }
        const auto v = ctx.value(o.text);
        if (!v || v->trimmed().isEmpty())
            return false;
        switch (kindOf(o)) {
        case Kind::Bool:
            return boolValue(*v).value_or(false);
        case Kind::Number: {
            const auto n = number(o, ctx.dimension ? ctx.dimension(o.text) : units::Dimension::None);
            return n && std::isfinite(*n) && *n != 0;
        }
        default:
            return true;
        }
    }

    bool compare(const Operand &lhs, const QString &op, const Operand &rhs) const
    {
        const Kind lk = kindOf(lhs), rk = kindOf(rhs);
        if (lk == Kind::Number || rk == Kind::Number) {
            const QString name = lk == Kind::Number ? lhs.text : rhs.text;
            const units::Dimension dim = ctx.dimension ? ctx.dimension(name) : units::Dimension::None;
            const auto a = number(lhs, dim);
            const auto b = number(rhs, dim);
            if (!a || !b)
                return op == "!=";
            if (op == "==")
                return std::abs(*a - *b) <= 1e-9 * std::max(1.0, std::abs(*a));
            if (op == "!=")
                return std::abs(*a - *b) > 1e-9 * std::max(1.0, std::abs(*a));
            if (op == "<")
                return *a < *b;
            if (op == "<=")
                return *a <= *b;
            if (op == ">")
                return *a > *b;
            return *a >= *b;
        }
        if (lk == Kind::Bool || rk == Kind::Bool) {
            const auto a = textOf(lhs), b = textOf(rhs);
            if (!a || !b)
                return op == "!=";
            const bool equal = boolValue(*a).value_or(false) == boolValue(*b).value_or(false);
            if (op == "==")
                return equal;
            if (op == "!=")
                return !equal;
            throw std::runtime_error("can't order true/false values");
        }
        const auto a = textOf(lhs), b = textOf(rhs);
        if (!a || !b)
            return op == "!=";
        const int c = a->compare(*b, Qt::CaseInsensitive);
        if (op == "==")
            return c == 0;
        if (op == "!=")
            return c != 0;
        if (op == "<")
            return c < 0;
        if (op == "<=")
            return c <= 0;
        if (op == ">")
            return c > 0;
        return c >= 0;
    }

    QList<Token> tokens;
    const Context &ctx;
    int pos = 0;
};

}  // namespace

bool evaluate(const QString &expression, const Context &context, bool fallback, QString *error)
{
    if (expression.trimmed().isEmpty())
        return true;
    try {
        Parser parser(tokenize(expression), context);
        return parser.parse();
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromStdString(e.what());
        return fallback;
    }
}

namespace {

std::optional<bool> boolText(const QString &v)
{
    const QString s = v.trimmed().toLower();
    if (s == "true" || s == "1" || s == "yes" || s == "on")
        return true;
    if (s == "false" || s == "0" || s == "no" || s == "off")
        return false;
    return std::nullopt;
}

bool isInfinite(const QString &v)
{
    const QString s = v.trimmed().toLower();
    return s == "inf" || s == "+inf" || s == "-inf";
}

}  // namespace

bool evaluate(const QJsonValue &expression, const Context &ctx, bool unknownValue, bool *sawUnknown)
{
    auto unknown = [&]() {
        if (sawUnknown)
            *sawUnknown = true;
        return unknownValue;
    };
    if (expression.isBool())
        return expression.toBool();
    if (!expression.isObject() || expression.toObject().size() != 1)
        return unknown();
    const QJsonObject node = expression.toObject();
    const QString op = node.begin().key();
    const QJsonValue arg = node.begin().value();

    if (op == "set") {
        const auto v = ctx.value(arg.toString());
        return v && !v->isEmpty();
    }
    if (op == "given")
        return ctx.given ? ctx.given(arg.toString()) : false;
    if (op == "finite") {
        const auto v = ctx.value(arg.toString());
        return v && !v->isEmpty() && !isInfinite(*v);
    }
    if (op == "equal") {
        const QJsonArray pair = arg.toArray();
        if (pair.size() != 2)
            return unknown();
        const QString name = pair.at(0).toString();
        const QString literal = pair.at(1).isString() ? pair.at(1).toString()
                                                      : pair.at(1).toVariant().toString();
        const auto v = ctx.value(name);
        if (!v)
            return false;
        if (ctx.kind && ctx.kind(name) == Kind::Bool)
            return boolText(*v) == boolText(literal);
        return v->trimmed().compare(literal, Qt::CaseInsensitive) == 0;
    }
    if (op == "not")
        return !evaluate(arg, ctx, !unknownValue, sawUnknown);
    if (op == "any" || op == "all") {
        const bool any = op == "any";
        for (const QJsonValue &e : arg.toArray()) {
            if (evaluate(e, ctx, unknownValue, sawUnknown) == any)
                return any;
        }
        return !any;
    }
    return unknown();
}

QStringList options(const QJsonValue &expression)
{
    QStringList out;
    if (!expression.isObject())
        return out;
    const QJsonObject node = expression.toObject();
    for (auto it = node.begin(); it != node.end(); ++it) {
        if (it.key() == "set" || it.key() == "given" || it.key() == "finite")
            out.append(it.value().toString());
        else if (it.key() == "equal")
            out.append(it.value().toArray().at(0).toString());
        else if (it.key() == "not")
            out += options(it.value());
        else if (it.key() == "any" || it.key() == "all")
            for (const QJsonValue &e : it.value().toArray())
                out += options(e);
    }
    out.removeDuplicates();
    return out;
}

std::optional<int> compareQuantities(const QString &a, const QString &b, units::Dimension dimension, bool metric)
{
    auto value = [&](const QString &text) -> std::optional<double> {
        if (isInfinite(text))
            return text.trimmed().startsWith('-') ? -std::numeric_limits<double>::infinity()
                                                  : std::numeric_limits<double>::infinity();
        const auto q = units::parse(text, dimension, metric);
        if (!q)
            return std::nullopt;
        return q->canonical;
    };
    const auto x = value(a), y = value(b);
    if (!x || !y)
        return std::nullopt;
    if (std::abs(*x - *y) <= 1e-9 * std::max({1.0, std::abs(*x), std::abs(*y)}))
        return 0;
    return *x < *y ? -1 : 1;
}

QStringList identifiers(const QString &expression)
{
    QStringList names;
    try {
        for (const Token &t : tokenize(expression)) {
            if (t.type == Token::Word && t.text != "true" && t.text != "false" && t.text != "inf" &&
                !names.contains(t.text))
                names.append(t.text);
        }
    } catch (const std::exception &) {
    }
    return names;
}

}  // namespace condition
