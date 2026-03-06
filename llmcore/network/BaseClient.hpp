/*
 * Copyright (C) 2026 Petr Mironychev
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

#include <QObject>
#include <QString>
#include <functional>

#include <llmcore/tools/ToolsManager.hpp>

namespace QodeAssist::LLMCore {

using ApiKey = std::function<QString()>;

class BaseClient : public QObject
{
    Q_OBJECT
public:
    explicit BaseClient(QObject *parent = nullptr);

    ToolsManager *tools() const;

signals:
    void partialResponseReceived(const QString &requestId, const QString &partialText);
    void fullResponseReceived(const QString &requestId, const QString &fullText);
    void requestFailed(const QString &requestId, const QString &error);
    void toolExecutionStarted(
        const QString &requestId, const QString &toolId, const QString &toolName);
    void toolExecutionCompleted(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QString &result);
    void continuationStarted(const QString &requestId);
    void thinkingBlockReceived(
        const QString &requestId, const QString &thinking, const QString &signature);
    void redactedThinkingBlockReceived(const QString &requestId, const QString &signature);

private:
    ToolsManager *m_toolsManager;
};

} // namespace QodeAssist::LLMCore
