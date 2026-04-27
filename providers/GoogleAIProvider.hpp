// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <pluginllmcore/Provider.hpp>

#include <LLMQore/GoogleAIClient.hpp>

namespace QodeAssist::Providers {

class GoogleAIProvider : public PluginLLMCore::Provider
{
    Q_OBJECT
public:
    explicit GoogleAIProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    void prepareRequest(
        QJsonObject &request,
        PluginLLMCore::PromptTemplate *prompt,
        PluginLLMCore::ContextData context,
        PluginLLMCore::RequestType type,
        bool isToolsEnabled,
        bool isThinkingEnabled) override;
    QFuture<QList<QString>> getInstalledModels(const QString &url) override;
    PluginLLMCore::ProviderID providerID() const override;
    PluginLLMCore::ProviderCapabilities capabilities() const override;

    PluginLLMCore::RequestID sendRequest(
        const QUrl &url, const QJsonObject &payload, const QString &endpoint) override;

    ::LLMQore::BaseClient *client() const override;
    QString apiKey() const override;

private:
    ::LLMQore::GoogleAIClient *m_client;
};

} // namespace QodeAssist::Providers
