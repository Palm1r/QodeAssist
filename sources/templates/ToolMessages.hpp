// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "llmcore/ContextData.hpp"

namespace QodeAssist::Templates {

inline bool appendOpenAIToolMessage(QJsonArray &messages, const LLMCore::Message &msg)
{
    if (!msg.toolCalls.isEmpty()) {
        QJsonArray toolCalls;
        for (const auto &call : msg.toolCalls) {
            toolCalls.append(QJsonObject{
                {"id", call.id},
                {"type", "function"},
                {"function",
                 QJsonObject{
                     {"name", call.name},
                     {"arguments",
                      QString::fromUtf8(
                          QJsonDocument(call.arguments).toJson(QJsonDocument::Compact))}}}});
        }
        QJsonObject toolMessage{{"role", "assistant"}, {"tool_calls", toolCalls}};
        toolMessage["content"] = msg.content.isEmpty() ? QJsonValue() : QJsonValue(msg.content);
        messages.append(toolMessage);
        return true;
    }

    if (msg.role == QLatin1String("tool")) {
        messages.append(QJsonObject{
            {"role", "tool"}, {"tool_call_id", msg.toolCallId}, {"content", msg.content}});
        return true;
    }

    return false;
}

} // namespace QodeAssist::Templates
