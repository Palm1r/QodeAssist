// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QString>

#include <variant>

namespace QodeAssist {

namespace ResponseEvents {

struct TextDelta
{
    QString text;
};

struct ThinkingDelta
{
    QString thinking;
    QString signature;
};

struct ToolCallStart
{
    QString id;
    QString name;
};

struct ToolCallArgsDelta
{
    QString id;
    QString jsonFragment;
};

struct ToolCallEnd
{
    QString id;
    QJsonObject finalArgs;
};

struct ToolResult
{
    QString toolUseId;
    QString text;
    bool isError = false;
};

struct Usage
{
    int inputTokens = 0;
    int outputTokens = 0;
};

struct Error
{
    QString message;
};

struct MessageStop
{
    QString stopReason;
};

} // namespace ResponseEvents

class ResponseEvent
{
public:
    enum class Kind {
        MessageStart,
        TextDelta,
        ThinkingDelta,
        ToolCallStart,
        ToolCallArgsDelta,
        ToolCallEnd,
        ToolResult,
        Usage,
        MessageStop,
        Error,
    };

    Kind kind() const noexcept { return m_kind; }

    template<typename T>
    const T *as() const noexcept
    {
        return std::get_if<T>(&m_data);
    }

    static ResponseEvent messageStart() { return {Kind::MessageStart, std::monostate{}}; }

    static ResponseEvent messageStop(QString stopReason = QString())
    {
        return {Kind::MessageStop, ResponseEvents::MessageStop{std::move(stopReason)}};
    }

    static ResponseEvent textDelta(QString text)
    {
        return {Kind::TextDelta, ResponseEvents::TextDelta{std::move(text)}};
    }

    static ResponseEvent thinkingDelta(QString thinking, QString signature = QString())
    {
        return {
            Kind::ThinkingDelta,
            ResponseEvents::ThinkingDelta{std::move(thinking), std::move(signature)}};
    }

    static ResponseEvent toolCallStart(QString id, QString name)
    {
        return {Kind::ToolCallStart, ResponseEvents::ToolCallStart{std::move(id), std::move(name)}};
    }

    static ResponseEvent toolCallArgsDelta(QString id, QString jsonFragment)
    {
        return {
            Kind::ToolCallArgsDelta,
            ResponseEvents::ToolCallArgsDelta{std::move(id), std::move(jsonFragment)}};
    }

    static ResponseEvent toolCallEnd(QString id, QJsonObject finalArgs)
    {
        return {
            Kind::ToolCallEnd, ResponseEvents::ToolCallEnd{std::move(id), std::move(finalArgs)}};
    }

    static ResponseEvent toolResult(QString toolUseId, QString text, bool isError = false)
    {
        return {
            Kind::ToolResult,
            ResponseEvents::ToolResult{std::move(toolUseId), std::move(text), isError}};
    }

    static ResponseEvent usage(int inputTokens, int outputTokens)
    {
        return {Kind::Usage, ResponseEvents::Usage{inputTokens, outputTokens}};
    }

    static ResponseEvent error(QString message)
    {
        return {Kind::Error, ResponseEvents::Error{std::move(message)}};
    }

private:
    using Data = std::variant<
        std::monostate,
        ResponseEvents::TextDelta,
        ResponseEvents::ThinkingDelta,
        ResponseEvents::ToolCallStart,
        ResponseEvents::ToolCallArgsDelta,
        ResponseEvents::ToolCallEnd,
        ResponseEvents::ToolResult,
        ResponseEvents::Usage,
        ResponseEvents::Error,
        ResponseEvents::MessageStop>;

    ResponseEvent(Kind kind, Data data)
        : m_kind(kind)
        , m_data(std::move(data))
    {}

    Kind m_kind;
    Data m_data;
};

} // namespace QodeAssist
