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

#include <llmcore/ContentBlocks.hpp>

namespace QodeAssist::Providers {

class GoogleMessage : public QObject
{
    Q_OBJECT
public:
    explicit GoogleMessage(QObject *parent = nullptr);

    void handleContentDelta(const QString &text);
    void handleThoughtDelta(const QString &text);
    void handleThoughtSignature(const QString &signature);
    void handleFunctionCallStart(const QString &name);
    void handleFunctionCallArgsDelta(const QString &argsJson);
    void handleFunctionCallComplete();
    void handleFinishReason(const QString &reason);

    QJsonObject toProviderFormat() const;
    QJsonArray createToolResultParts(const QHash<QString, QString> &toolResults) const;

    QList<LLMCore::ToolUseContent *> getCurrentToolUseContent() const;
    QList<LLMCore::ThinkingContent *> getCurrentThinkingContent() const;
    QList<LLMCore::ContentBlock *> currentBlocks() const { return m_currentBlocks; }

    LLMCore::MessageState state() const { return m_state; }
    QString finishReason() const { return m_finishReason; }
    bool isErrorFinishReason() const;
    QString getErrorMessage() const;
    void startNewContinuation();

private:
    void updateStateFromFinishReason();

    QList<LLMCore::ContentBlock *> m_currentBlocks;
    QString m_pendingFunctionArgs;
    QString m_currentFunctionName;
    QString m_finishReason;
    LLMCore::MessageState m_state = LLMCore::MessageState::Building;
};

} // namespace QodeAssist::Providers
