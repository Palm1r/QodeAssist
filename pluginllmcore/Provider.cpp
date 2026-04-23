// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Provider.hpp"

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ToolsManager.hpp>

#include <QJsonDocument>

#include <Logger.hpp>

namespace QodeAssist::PluginLLMCore {

Provider::Provider(QObject *parent)
    : QObject(parent)
{}

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

} // namespace QodeAssist::PluginLLMCore
