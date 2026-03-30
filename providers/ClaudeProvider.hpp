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

#include <pluginllmcore/Provider.hpp>

#include "ClaudeMessage.hpp"
#include <LLMCore/ClaudeClient.hpp>

namespace QodeAssist::Providers {

class ClaudeProvider : public PluginLLMCore::Provider
{
    Q_OBJECT
public:
    explicit ClaudeProvider(QObject *parent = nullptr);

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
    bool supportThinking() const override;
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
    void processStreamEvent(const QString &requestId, const QJsonObject &event);
    void handleMessageComplete(const QString &requestId);
    void cleanupRequest(const PluginLLMCore::RequestID &requestId);

    QHash<QodeAssist::PluginLLMCore::RequestID, ClaudeMessage *> m_messages;
    QHash<QodeAssist::PluginLLMCore::RequestID, QUrl> m_requestUrls;
    QHash<QodeAssist::PluginLLMCore::RequestID, QJsonObject> m_originalRequests;
    ::LLMCore::ClaudeClient *m_client;
};

} // namespace QodeAssist::Providers
