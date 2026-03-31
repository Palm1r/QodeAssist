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

#include <QSet>

#include <LLMCore/OpenAIClient.hpp>
#include <pluginllmcore/Provider.hpp>

namespace QodeAssist::Providers {

class OpenAICompatProvider : public PluginLLMCore::Provider
{
    Q_OBJECT
public:
    explicit OpenAICompatProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    QString completionEndpoint() const override;
    QString chatEndpoint() const override;
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

    PluginLLMCore::ProviderCapabilities capabilities() const override;
    void cancelRequest(const PluginLLMCore::RequestID &requestId) override;

    ::LLMCore::ToolsManager *toolsManager() const override;

private:
    ::LLMCore::OpenAIClient *m_client;
    QHash<PluginLLMCore::RequestID, ::LLMCore::RequestID> m_providerToClientIds;
    QHash<::LLMCore::RequestID, PluginLLMCore::RequestID> m_clientToProviderIds;
    QSet<PluginLLMCore::RequestID> m_awaitingContinuation;
};

} // namespace QodeAssist::Providers
