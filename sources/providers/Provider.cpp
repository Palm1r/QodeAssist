// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Provider.hpp"

#include "PromptTemplate.hpp"

#include <LLMQore/BaseClient.hpp>
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
    bool isThinkingEnabled)
{
    if (!prompt) {
        LOG_MESSAGE(QString("Provider '%1': null template").arg(name()));
        return false;
    }

    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template '%1' doesn't support provider '%2'")
                        .arg(prompt->name(), name()));
        return false;
    }

    if (!prompt->buildFullRequest(request, context, isThinkingEnabled)) {
        LOG_MESSAGE(
            QString("Provider '%1': template '%2' failed to build request")
                .arg(name(), prompt->name()));
        return false;
    }

    if (isToolsEnabled) {
        const auto toolsDefinitions = toolsManager()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
        }
    }
    return true;
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
