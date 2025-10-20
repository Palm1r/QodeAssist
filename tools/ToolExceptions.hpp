/* 
 * Copyright (C) 2025 Petr Mironychev
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

#pragma once

#include <QException>
#include <QString>

namespace QodeAssist::Tools {

class ToolException : public QException
{
public:
    explicit ToolException(const QString &message)
        : m_message(message)
        , m_stdMessage(message.toStdString())
    {}

    void raise() const override { throw *this; }
    ToolException *clone() const override { return new ToolException(*this); }
    const char *what() const noexcept override { return m_stdMessage.c_str(); }
    
    QString message() const { return m_message; }

private:
    QString m_message;
    std::string m_stdMessage;
};

class ToolRuntimeError : public ToolException
{
public:
    explicit ToolRuntimeError(const QString &message)
        : ToolException(message)
    {}

    void raise() const override { throw *this; }
    ToolRuntimeError *clone() const override { return new ToolRuntimeError(*this); }
};

class ToolInvalidArgument : public ToolException
{
public:
    explicit ToolInvalidArgument(const QString &message)
        : ToolException(message)
    {}

    void raise() const override { throw *this; }
    ToolInvalidArgument *clone() const override { return new ToolInvalidArgument(*this); }
};

} // namespace QodeAssist::Tools

