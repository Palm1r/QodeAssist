// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace QodeAssist::Providers::ClaudeCacheControl {

inline QJsonObject buildBreakpoint(bool extendedTtl)
{
    QJsonObject cacheControl{{"type", "ephemeral"}};
    if (extendedTtl)
        cacheControl["ttl"] = "1h";
    return cacheControl;
}

inline void markLastBlock(QJsonArray &blocks, const QJsonObject &cacheControl)
{
    if (blocks.isEmpty())
        return;
    QJsonObject last = blocks.last().toObject();
    last["cache_control"] = cacheControl;
    blocks.replace(blocks.size() - 1, last);
}

inline void applyToSystem(QJsonObject &request, const QJsonObject &cacheControl)
{
    if (!request.contains("system"))
        return;

    const QJsonValue sys = request.value("system");
    if (sys.isString()) {
        const QString text = sys.toString();
        if (!text.isEmpty()) {
            request["system"] = QJsonArray{QJsonObject{
                {"type", "text"}, {"text", text}, {"cache_control", cacheControl}}};
        }
    } else if (sys.isArray()) {
        QJsonArray blocks = sys.toArray();
        markLastBlock(blocks, cacheControl);
        request["system"] = blocks;
    }
}

inline void applyToTools(QJsonObject &request, const QJsonObject &cacheControl)
{
    if (!request.contains("tools"))
        return;
    QJsonArray tools = request.value("tools").toArray();
    markLastBlock(tools, cacheControl);
    request["tools"] = tools;
}

inline void applyToHistory(QJsonObject &request, const QJsonObject &cacheControl)
{
    if (!request.contains("messages"))
        return;
    QJsonArray messages = request.value("messages").toArray();
    if (messages.size() < 2)
        return;

    const int idx = messages.size() - 2;
    QJsonObject msg = messages[idx].toObject();
    const QJsonValue content = msg.value("content");
    if (content.isString()) {
        msg["content"] = QJsonArray{QJsonObject{
            {"type", "text"}, {"text", content.toString()}, {"cache_control", cacheControl}}};
    } else if (content.isArray()) {
        QJsonArray blocks = content.toArray();
        markLastBlock(blocks, cacheControl);
        msg["content"] = blocks;
    }
    messages.replace(idx, msg);
    request["messages"] = messages;
}

inline void apply(QJsonObject &request, bool extendedTtl)
{
    const QJsonObject cacheControl = buildBreakpoint(extendedTtl);
    applyToSystem(request, cacheControl);
    applyToTools(request, cacheControl);
    applyToHistory(request, cacheControl);
}

} // namespace QodeAssist::Providers::ClaudeCacheControl
