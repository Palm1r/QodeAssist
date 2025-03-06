/*
 * Copyright (C) 2025 Povilas Kanapickas
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <llmcore/ContextData.hpp>
#include <QString>

QT_BEGIN_NAMESPACE

// gtest can't pick pretty printer when comparing QString
inline void PrintTo(const QString &value, ::std::ostream *out)
{
    *out << '"' << value.toStdString() << '"';
}

QT_END_NAMESPACE

inline std::ostream &operator<<(std::ostream &out, const QString &value)
{
    out << '"' << value.toStdString() << '"';
    return out;
}

template<class T>
std::ostream &operator<<(std::ostream &out, const QVector<T> &value)
{
    out << "[";
    for (const auto &el : value) {
        out << value << ", ";
    }
    out << "]";
    return out;
}

template<class T>
std::ostream &operator<<(std::ostream &out, const std::optional<T> &value)
{
    if (value.has_value()) {
        out << value.value();
    } else {
        out << "(no value)";
    }
    return out;
}

namespace QodeAssist::LLMCore {

inline std::ostream &operator<<(std::ostream &out, const Message &value)
{
    out << "Message{"
        << "role=" << value.role << "content=" << value.content << "}";
    return out;
}

inline std::ostream &operator<<(std::ostream &out, const ContextData &value)
{
    out << "ContextData{"
        << "\n  systemPrompt=" << value.systemPrompt << "\n  prefix=" << value.prefix
        << "\n  suffix=" << value.suffix << "\n  fileContext=" << value.fileContext
        << "\n  history=" << value.history << "\n}";
    return out;
}

} // namespace QodeAssist::LLMCore
