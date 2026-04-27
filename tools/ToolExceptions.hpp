// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

