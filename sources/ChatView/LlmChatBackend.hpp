// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QHash>
#include <QString>

#include <LLMQore/BaseClient.hpp>

#include "providers/Provider.hpp"
#include "session/ChatBackend.hpp"
#include "templates/IPromptProvider.hpp"

namespace QodeAssist::Chat {

class LlmChatBackend : public Session::ChatBackend
{
    Q_OBJECT

public:
    explicit LlmChatBackend(Templates::IPromptProvider *promptProvider, QObject *parent = nullptr);
    ~LlmChatBackend() override;

    void sendTurn(const Session::TurnRequest &request) override;
    void cancel() override;
    Session::TurnContextNeeds contextNeeds() const override;

    void setChatFilePath(const QString &filePath) override;
    void clearToolSession(const QString &filePath) override;

private:
    void connectClient(Providers::Provider *provider);
    void releaseRequest();
    void bindToolSessions(Providers::Provider *provider);
    QVector<LLMCore::Message> renderHistory(
        const Session::ConversationHistory &history,
        Providers::Provider *provider,
        Templates::PromptTemplate *promptTemplate) const;
    QVector<LLMCore::ImageAttachment> loadImagesFromStorage(
        const QList<Session::ImageBlock> &storedImages) const;

    void handleChunk(const QString &requestId, const QString &chunk);
    void handleCompleted(const QString &requestId, const QString &fullText);
    void handleFinalized(
        const ::LLMQore::RequestID &requestId, const ::LLMQore::CompletionInfo &info);
    void handleFailed(const QString &requestId, const QString &error);
    void handleThinkingBlock(
        const QString &requestId, const QString &thinking, const QString &signature);
    void handleToolStarted(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QJsonObject &arguments);
    void handleToolResult(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QString &toolOutput);

    Templates::IPromptProvider *m_promptProvider = nullptr;
    QString m_chatFilePath;

    Providers::Provider *m_provider = nullptr;
    QString m_requestId;
    bool m_dropPreToolText = false;
};

} // namespace QodeAssist::Chat
