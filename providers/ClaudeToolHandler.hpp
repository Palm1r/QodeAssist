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
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "llmcore/RequestType.hpp"
#include "tools/ToolsFactory.hpp"

namespace QodeAssist::Providers {

class ClaudeToolHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClaudeToolHandler(QObject *parent = nullptr);

    void setToolsFactory(Tools::ToolsFactory *toolsFactory);

    void executeTool(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &input);

    void cleanupRequest(const QString &requestId);

signals:
    void toolCompleted(const QString &requestId, const QString &toolId, const QString &result);
    void toolFailed(const QString &requestId, const QString &toolId, const QString &error);

private slots:
    void onToolCompleted(const QString &toolName, const QString &result);
    void onToolFailed(const QString &toolName, const QString &error);

private:
    struct ToolExecution
    {
        QString requestId;
        QString toolId;
        QString toolName;
        QDateTime startTime;
    };

    Tools::ToolsFactory *m_toolsFactory = nullptr;
    QHash<QString, ToolExecution> m_activeTools; // toolName -> execution info
};

} // namespace QodeAssist::Providers
