// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include "Provider.hpp"

namespace LLMQore {
class BaseClient;
}

namespace QodeAssist::Providers {

// A configuration-driven provider: it owns an LLMQore client and exposes a
// fixed identity/capability set. Concrete behaviour (request shape) comes from
// the agent's prompt template via Provider::prepareRequest, so a single class
// covers every client_api by varying the client factory + metadata.
class GenericProvider : public Provider
{
    Q_OBJECT
public:
    using ClientFactory = std::function<::LLMQore::BaseClient *(QObject *)>;

    GenericProvider(
        QString name,
        ProviderID id,
        ProviderCapabilities capabilities,
        const ClientFactory &clientFactory,
        QObject *parent = nullptr);

    QString name() const override;
    QFuture<QList<QString>> getInstalledModels(const QString &url) override;
    ProviderID providerID() const override;
    ProviderCapabilities capabilities() const override;
    ::LLMQore::BaseClient *client() const override;

private:
    QString m_name;
    ProviderID m_id;
    ProviderCapabilities m_capabilities;
    ::LLMQore::BaseClient *m_client;
};

// Registers every built-in client_api into ProviderFactory. Must be called once
// at plugin startup before any agent/session is created.
void registerBuiltinProviders();

} // namespace QodeAssist::Providers
