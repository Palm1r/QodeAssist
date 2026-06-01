// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "GenericProvider.hpp"

#include <utility>

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
    QString name,
    ProviderID id,
    ProviderCapabilities capabilities,
    const ClientFactory &clientFactory,
    QObject *parent)
    : Provider(parent)
    , m_name(std::move(name))
    , m_id(id)
    , m_capabilities(capabilities)
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

ProviderCapabilities GenericProvider::capabilities() const
{
    return m_capabilities;
}

::LLMQore::BaseClient *GenericProvider::client() const
{
    return m_client;
}

QFuture<QList<QString>> GenericProvider::getInstalledModels(const QString &url)
{
    m_client->setUrl(url);
    m_client->setApiKey(apiKey());
    return m_client->listModels();
}

namespace {

using Cap = ProviderCapability;

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
                        ProviderCapabilities caps,
                        GenericProvider::ClientFactory factory) {
        ProviderFactory::registerType(api, [=](QObject *parent) -> Provider * {
            return new GenericProvider(api, id, caps, factory, parent);
        });
    };

    const ProviderCapabilities full
        = Cap::Tools | Cap::Thinking | Cap::Image | Cap::ModelListing;

    reg("Claude", ProviderID::Claude, full, makeFactory<::LLMQore::ClaudeClient>());
    reg("Google AI", ProviderID::GoogleAI, full, makeFactory<::LLMQore::GoogleAIClient>());
    reg("llama.cpp", ProviderID::LlamaCpp, full, makeFactory<::LLMQore::LlamaCppClient>());
    reg("LM Studio (Chat Completions)", ProviderID::LMStudio, full,
        makeFactory<::LLMQore::OpenAIClient>());
    reg("LM Studio (Responses API)", ProviderID::OpenAIResponses, full,
        makeFactory<::LLMQore::OpenAIResponsesClient>());
    reg("Mistral AI", ProviderID::MistralAI, full, makeFactory<::LLMQore::MistralClient>());
    reg("Codestral", ProviderID::MistralAI, Cap::Tools | Cap::Image,
        makeFactory<::LLMQore::MistralClient>());
    reg("Ollama (Native)", ProviderID::Ollama, full, makeFactory<::LLMQore::OllamaClient>());
    reg("Ollama (OpenAI-compatible)", ProviderID::OpenAICompatible, full,
        makeFactory<::LLMQore::OpenAIClient>());
    reg("OpenAI (Chat Completions)", ProviderID::OpenAI, full,
        makeFactory<::LLMQore::OpenAIClient>());
    reg("OpenAI (Responses API)", ProviderID::OpenAIResponses, full,
        makeFactory<::LLMQore::OpenAIResponsesClient>());
    reg("OpenAI Compatible", ProviderID::OpenAICompatible,
        Cap::Tools | Cap::Image | Cap::Thinking, makeFactory<::LLMQore::OpenAIClient>());
    reg("OpenRouter", ProviderID::OpenRouter,
        Cap::Tools | Cap::Image | Cap::Thinking | Cap::ModelListing,
        makeFactory<::LLMQore::OpenAIClient>());
}

} // namespace QodeAssist::Providers
