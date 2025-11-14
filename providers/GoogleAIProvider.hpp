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

#include "GoogleMessage.hpp"
#include "llmcore/Provider.hpp"
#include "tools/ToolsManager.hpp"

namespace QodeAssist::Providers {

class GoogleAIProvider : public LLMCore::Provider
{
    Q_OBJECT
public:
    explicit GoogleAIProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString completionEndpoint() const override;
    QString chatEndpoint() const override;
    bool supportsModelListing() const override;
    void prepareRequest(
        QJsonObject &request,
        LLMCore::PromptTemplate *prompt,
        LLMCore::ContextData context,
        LLMCore::RequestType type,
        bool isToolsEnabled,
        bool isThinkingEnabled) override;
    QList<QString> getInstalledModels(const QString &url) override;
    QList<QString> validateRequest(const QJsonObject &request, LLMCore::TemplateType type) override;
    QString apiKey() const override;
    void prepareNetworkRequest(QNetworkRequest &networkRequest) const override;
    LLMCore::ProviderID providerID() const override;

    void sendRequest(
        const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload) override;

    bool supportsTools() const override;
    bool supportThinking() const override;
    void cancelRequest(const LLMCore::RequestID &requestId) override;

public slots:
    void onDataReceived(
        const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data) override;
    void onRequestFinished(
        const QodeAssist::LLMCore::RequestID &requestId,
        bool success,
        const QString &error) override;

private slots:
    void onToolExecutionComplete(
        const QString &requestId, const QHash<QString, QString> &toolResults);

private:
    void processStreamChunk(const QString &requestId, const QJsonObject &chunk);
    void handleMessageComplete(const QString &requestId);
    void emitPendingThinkingBlocks(const QString &requestId);
    void cleanupRequest(const LLMCore::RequestID &requestId);

    QHash<LLMCore::RequestID, GoogleMessage *> m_messages;
    QHash<LLMCore::RequestID, QUrl> m_requestUrls;
    QHash<LLMCore::RequestID, QJsonObject> m_originalRequests;
    QHash<LLMCore::RequestID, int> m_emittedThinkingBlocksCount;
    QSet<LLMCore::RequestID> m_failedRequests;
    Tools::ToolsManager *m_toolsManager;
};

} // namespace QodeAssist::Providers
