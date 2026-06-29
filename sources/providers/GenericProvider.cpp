// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "GenericProvider.hpp"

#include <utility>

#include <QJsonObject>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ClaudeClient.hpp>
#include <LLMQore/GoogleAIClient.hpp>
#include <LLMQore/LlamaCppClient.hpp>
#include <LLMQore/MistralClient.hpp>
#include <LLMQore/OllamaClient.hpp>
#include <LLMQore/OpenAIClient.hpp>
#include <LLMQore/OpenAIResponsesClient.hpp>

#include "ProviderFactory.hpp"

namespace QodeAssist::Providers {

GenericProvider::GenericProvider(
    QString name, ProviderID id, const ClientFactory &clientFactory, QObject *parent)
    : Provider(parent)
    , m_name(std::move(name))
    , m_id(id)
    , m_client(clientFactory(this))
{}

QString GenericProvider::name() const
{
    return m_name;
}

ProviderID GenericProvider::providerID() const
{
    return m_id;
}

::LLMQore::BaseClient *GenericProvider::client() const
{
    return m_client;
}

QFuture<QList<QString>> GenericProvider::getInstalledModels(const QString &url)
{
    m_client->setUrl(url);
    m_client->setApiKey(apiKey());
    return m_client->listModels(modelsEndpoint(url));
}

QString GenericProvider::modelsEndpoint(const QString &url) const
{
    switch (m_id) {
    case ProviderID::OpenAI:
    case ProviderID::OpenAIResponses:
    case ProviderID::OpenAICompatible:
    case ProviderID::LMStudio:
    case ProviderID::OpenRouter:
        break;
    default:
        return {};
    }

    QString base = url;
    while (base.endsWith('/'))
        base.chop(1);
    return base.endsWith("/v1") ? QStringLiteral("/models") : QStringLiteral("/v1/models");
}

RequestID GenericProvider::sendRequest(
    const QUrl &url, const QJsonObject &payload, const QString &endpoint)
{
    // Gemini carries the model in the URL and rejects unknown body fields, so
    // the model/stream keys injected by the generic pipeline must be dropped.
    if (m_id == ProviderID::GoogleAI) {
        QJsonObject cleaned = payload;
        cleaned.remove("model");
        cleaned.remove("stream");
        return Provider::sendRequest(url, cleaned, endpoint);
    }
    return Provider::sendRequest(url, payload, endpoint);
}

namespace {

template<typename ClientT>
GenericProvider::ClientFactory makeFactory()
{
    return [](QObject *parent) -> ::LLMQore::BaseClient * {
        return new ClientT(QString(), QString(), QString(), parent);
    };
}

} // namespace

void registerBuiltinProviders()
{
    const auto reg = [](const QString &api,
                        ProviderID id,
                        GenericProvider::ClientFactory factory) {
        ProviderFactory::registerType(api, [=](QObject *parent) -> Provider * {
            return new GenericProvider(api, id, factory, parent);
        });
    };

    reg("Claude", ProviderID::Claude, makeFactory<::LLMQore::ClaudeClient>());
    reg("Google AI", ProviderID::GoogleAI, makeFactory<::LLMQore::GoogleAIClient>());
    reg("llama.cpp", ProviderID::LlamaCpp, makeFactory<::LLMQore::LlamaCppClient>());
    reg("LM Studio (Chat Completions)", ProviderID::LMStudio,
        makeFactory<::LLMQore::OpenAIClient>());
    reg("LM Studio (Responses API)", ProviderID::OpenAIResponses,
        makeFactory<::LLMQore::OpenAIResponsesClient>());
    reg("Mistral AI", ProviderID::MistralAI, makeFactory<::LLMQore::MistralClient>());
    reg("Codestral", ProviderID::MistralAI, makeFactory<::LLMQore::MistralClient>());
    reg("Ollama (Native)", ProviderID::Ollama, makeFactory<::LLMQore::OllamaClient>());
    reg("Ollama (OpenAI-compatible)", ProviderID::OpenAICompatible,
        makeFactory<::LLMQore::OpenAIClient>());
    reg("OpenAI (Chat Completions)", ProviderID::OpenAI,
        makeFactory<::LLMQore::OpenAIClient>());
    reg("OpenAI (Responses API)", ProviderID::OpenAIResponses,
        makeFactory<::LLMQore::OpenAIResponsesClient>());
    reg("OpenAI Compatible", ProviderID::OpenAICompatible,
        makeFactory<::LLMQore::OpenAIClient>());
    reg("OpenRouter", ProviderID::OpenRouter, makeFactory<::LLMQore::OpenAIClient>());
}

} // namespace QodeAssist::Providers
