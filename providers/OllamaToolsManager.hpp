/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QString>

#include "ClaudeToolHandler.hpp"
#include "tools/ToolsFactory.hpp"

namespace QodeAssist::Providers {

class OllamaToolsManager : public QObject
{
    Q_OBJECT

public:
    explicit OllamaToolsManager(QObject *parent = nullptr);

    void setToolsFactory(Tools::ToolsFactory *toolsFactory);

    // Process Ollama response events - returns text for user
    QString processEvent(const QString &requestId, const QJsonObject &chunk);

    // Request lifecycle
    void initializeRequest(const QString &requestId, const QJsonObject &originalRequest);
    void cleanupRequest(const QString &requestId);

    // Tools support
    bool hasToolsSupport() const;
    QJsonArray getToolsDefinitions() const;
    bool hasActiveTools(const QString &requestId) const;

signals:
    void requestReadyForContinuation(const QString &requestId, const QJsonObject &followUpRequest);

private slots:
    void onToolCompleted(const QString &requestId, const QString &toolId, const QString &result);
    void onToolFailed(const QString &requestId, const QString &toolId, const QString &error);

private:
    struct ToolCall
    {
        QString id;
        QString name;
        QJsonObject arguments;
        bool isExecuted = false;
    };

    struct RequestState
    {
        QJsonObject originalRequest;
        QJsonArray originalMessages;
        QString assistantText;
        QList<ToolCall> toolCalls;           // Ordered list of tool calls
        QHash<QString, QString> toolResults; // toolId -> result
        QQueue<QString> pendingToolIds;      // Queue for sequential execution
        QString currentExecutingToolId;      // Currently executing tool
        bool toolCallsReceived = false;      // Tool calls received from response

        RequestState() = default;
        RequestState(const QJsonObject &request)
            : originalRequest(request)
            , originalMessages(request["messages"].toArray())
        {}

        bool allToolsCompleted() const
        {
            return toolCallsReceived && pendingToolIds.isEmpty()
                   && currentExecutingToolId.isEmpty();
        }

        bool hasActiveTools() const
        {
            return !pendingToolIds.isEmpty() || !currentExecutingToolId.isEmpty();
        }
    };

    void processNextTool(const QString &requestId);
    void executeToolCall(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &input);
    void sendContinuationRequest(const QString &requestId);

    Tools::ToolsFactory *m_toolsFactory = nullptr;
    std::unique_ptr<ClaudeToolHandler> m_toolHandler;
    QHash<QString, RequestState> m_requestStates;
};

} // namespace QodeAssist::Providers
