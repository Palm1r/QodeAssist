// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/OpenAIClient.hpp>
#include <pluginllmcore/Provider.hpp>

namespace QodeAssist::Providers {

class LMStudioProvider : public PluginLLMCore::Provider
{
    Q_OBJECT
public:
    explicit LMStudioProvider(QObject *parent = nullptr);

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

    ::LLMQore::BaseClient *client() const override;
    QString apiKey() const override;

private:
    ::LLMQore::OpenAIClient *m_client;
};

} // namespace QodeAssist::Providers
