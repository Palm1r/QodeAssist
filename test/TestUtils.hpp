// Copyright (C) 2025 Povilas Kanapickas
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <iostream>
#include <sources/common/ContextData.hpp>
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

namespace QodeAssist::Templates {

inline std::ostream &operator<<(std::ostream &out, const Message &value)
{
    out << "Message{role=" << value.role << ", blocks=" << value.blocks.size() << "}";
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

} // namespace QodeAssist::Templates
