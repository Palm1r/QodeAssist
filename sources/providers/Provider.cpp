// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "Provider.hpp"

#include "ClaudeCacheControl.hpp"
#include "PromptTemplate.hpp"

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ClaudeClient.hpp>
#include <LLMQore/ToolsManager.hpp>

#include <QJsonArray>
#include <QJsonDocument>

#include <Logger.hpp>

namespace QodeAssist::Providers {

Provider::Provider(QObject *parent)
    : QObject(parent)
{}

bool Provider::prepareRequest(
    QJsonObject &request,
    PromptTemplate *prompt,
    const ContextData &context,
    bool isToolsEnabled,
    QString *errorOut)
{
    const auto fail = [errorOut](const QString &message) {
        LOG_MESSAGE(message);
        if (errorOut)
            *errorOut = message;
        return false;
    };

    if (!prompt)
        return fail(QString("Provider '%1': null template").arg(name()));

    if (!prompt->isSupportProvider(providerID())) {
        return fail(QString("Template '%1' doesn't support provider '%2'")
                        .arg(prompt->name(), name()));
    }

    if (!prompt->buildFullRequest(request, context)) {
        return fail(
            QString("Provider '%1': template '%2' failed to build request (see log)")
                .arg(name(), prompt->name()));
    }

    if (isToolsEnabled) {
        const auto toolsDefinitions = toolsManager()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
        }
    }

    if (m_promptCachingEnabled)
        ClaudeCacheControl::apply(request, m_promptCachingExtendedTtl);

    return true;
}

void Provider::setPromptCaching(bool enabled, bool extendedTtl)
{
    m_promptCachingEnabled = enabled;
    m_promptCachingExtendedTtl = enabled && extendedTtl;
    if (auto *claude = qobject_cast<::LLMQore::ClaudeClient *>(client()))
        claude->setUseExtendedCacheTTL(m_promptCachingExtendedTtl);
}

RequestID Provider::sendRequest(
    const QUrl &url, const QJsonObject &payload, const QString &endpoint)
{
    auto *c = client();

    c->setUrl(url.toString());
    c->setApiKey(apiKey());

    auto requestId = c->sendMessage(payload, endpoint);

    LOG_MESSAGE(
        QString("%1: Sending request %2 to %3%4").arg(name(), requestId, url.toString(), endpoint));
    LOG_MESSAGE(
        QString("%1: Payload:\n%2")
            .arg(name(), QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Indented))));

    return requestId;
}

void Provider::cancelRequest(const RequestID &requestId)
{
    LOG_MESSAGE(QString("%1: Cancelling request %2").arg(name(), requestId));
    client()->cancelRequest(requestId);
}

::LLMQore::ToolsManager *Provider::toolsManager() const
{
    return client()->tools();
}

} // namespace QodeAssist::Providers
