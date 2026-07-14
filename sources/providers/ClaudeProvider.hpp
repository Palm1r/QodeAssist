// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "providers/Provider.hpp"

#include <LLMQore/ClaudeClient.hpp>

namespace QodeAssist::Providers {

class ClaudeProvider : public Providers::Provider
{
    Q_OBJECT
public:
    explicit ClaudeProvider(QObject *parent = nullptr);

    QString name() const override;
    QString url() const override;
    void prepareRequest(
        QJsonObject &request,
        Templates::PromptTemplate *prompt,
        LLMCore::ContextData context,
        LLMCore::RequestType type,
        bool isToolsEnabled,
        bool isThinkingEnabled) override;
    QFuture<QList<QString>> getInstalledModels(const QString &url) override;
    Providers::ProviderID providerID() const override;
    Providers::ProviderCapabilities capabilities() const override;

    ::LLMQore::BaseClient *client() const override;
    QString apiKey() const override;

private:
    ::LLMQore::ClaudeClient *m_client;
};

} // namespace QodeAssist::Providers
