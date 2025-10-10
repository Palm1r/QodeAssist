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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include "ToolHandler.hpp"
#include "ToolsFactory.hpp"
#include <llmcore/BaseTool.hpp>

namespace QodeAssist::Tools {

struct PendingTool
{
    QString id;
    QString name;
    QJsonObject input;
    QString result;
    bool complete = false;
};

class ToolsManager : public QObject
{
    Q_OBJECT

public:
    explicit ToolsManager(QObject *parent = nullptr);

    void executeToolCall(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &input);

    QJsonArray getToolsDefinitions(LLMCore::ToolSchemaFormat format) const;
    void cleanupRequest(const QString &requestId);

    ToolsFactory *toolsFactory() const;

signals:
    void toolExecutionComplete(const QString &requestId, const QHash<QString, QString> &toolResults);

private slots:
    void onToolFinished(
        const QString &requestId, const QString &toolId, const QString &result, bool success);

private:
    ToolsFactory *m_toolsFactory;
    ToolHandler *m_toolHandler;
    QHash<QString, QHash<QString, PendingTool>> m_pendingTools;

    bool isExecutionComplete(const QString &requestId) const;
    QHash<QString, QString> getToolResults(const QString &requestId) const;
};

} // namespace QodeAssist::Tools
