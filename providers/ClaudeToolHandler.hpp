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
#include <QString>

#include "tools/ToolsFactory.hpp"

namespace QodeAssist::Providers {

struct RequestState
{
    QString assistantText;
    QString currentToolId;
    QString currentToolName;
    QJsonObject currentToolInput;
    QJsonObject originalRequest;

    bool hasActiveTool() const { return !currentToolId.isEmpty(); }

    void clearTool()
    {
        currentToolId.clear();
        currentToolName.clear();
        currentToolInput = QJsonObject();
    }
};

class ClaudeToolHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClaudeToolHandler(QObject *parent = nullptr);

    void setToolsFactory(Tools::ToolsFactory *toolsFactory);

    // Request state management
    void initializeRequest(const QString &requestId, const QJsonObject &request);
    void clearRequest(const QString &requestId);

    // Tool processing
    void processToolStart(
        const QString &requestId,
        const QString &toolName,
        const QString &toolId,
        const QJsonObject &input);
    void addAssistantText(const QString &requestId, const QString &text);

    // Check if request has active tools
    bool hasActiveTool(const QString &requestId) const;

    // Get updated request with tool result
    QJsonObject buildToolResultRequest(
        const QString &requestId, const QString &toolId, const QString &result);

signals:
    void toolResultReady(const QString &requestId, const QJsonObject &newRequest);

private slots:
    void executeToolAsync(
        const QString &requestId,
        const QString &toolName,
        const QString &toolId,
        const QJsonObject &input);

private:
    Tools::ToolsFactory *m_toolsFactory = nullptr;
    QHash<QString, RequestState> m_requestStates;
};

} // namespace QodeAssist::Providers
