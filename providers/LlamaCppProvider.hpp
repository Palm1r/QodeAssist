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

#include "OpenAIMessage.hpp"
#include <LLMCore/LlamaCppClient.hpp>
#include <pluginllmcore/Provider.hpp>

namespace QodeAssist::Providers {

class LlamaCppProvider : public PluginLLMCore::Provider
{
    Q_OBJECT
public:
    explicit LlamaCppProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString completionEndpoint() const override;
    QString chatEndpoint() const override;
    bool supportsModelListing() const override;
    void prepareRequest(
        QJsonObject &request,
        PluginLLMCore::PromptTemplate *prompt,
        PluginLLMCore::ContextData context,
        PluginLLMCore::RequestType type,
        bool isToolsEnabled,
        bool isThinkingEnabled) override;
    QFuture<QList<QString>> getInstalledModels(const QString &url) override;
    QList<QString> validateRequest(const QJsonObject &request, PluginLLMCore::TemplateType type) override;
    QString apiKey() const override;
    void prepareNetworkRequest(QNetworkRequest &networkRequest) const override;
    PluginLLMCore::ProviderID providerID() const override;

    void sendRequest(
        const PluginLLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload) override;

    bool supportsTools() const override;
    bool supportImage() const override;
    void cancelRequest(const PluginLLMCore::RequestID &requestId) override;

    ::LLMCore::ToolsManager *toolsManager() const override;

public slots:
    void onDataReceived(
        const QodeAssist::PluginLLMCore::RequestID &requestId, const QByteArray &data) override;
    void onRequestFinished(
        const QodeAssist::PluginLLMCore::RequestID &requestId,
        std::optional<QString> error) override;

private slots:
    void onToolExecutionComplete(
        const QString &requestId, const QHash<QString, QString> &toolResults);

private:
    void processStreamChunk(const QString &requestId, const QJsonObject &chunk);
    void handleMessageComplete(const QString &requestId);
    void cleanupRequest(const PluginLLMCore::RequestID &requestId);

    QHash<PluginLLMCore::RequestID, OpenAIMessage *> m_messages;
    QHash<PluginLLMCore::RequestID, QUrl> m_requestUrls;
    QHash<PluginLLMCore::RequestID, QJsonObject> m_originalRequests;
    ::LLMCore::LlamaCppClient *m_client;
};

} // namespace QodeAssist::Providers
