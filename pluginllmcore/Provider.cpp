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

#include "Provider.hpp"

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/LlamaCppClient.hpp>
#include <LLMQore/OpenAIClient.hpp>
#include <LLMQore/OpenAIResponsesClient.hpp>
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

QString Provider::enrichErrorMessage(const QString &error) const
{
    auto *c = client();

    const bool isOpenAICompatible = qobject_cast<::LLMQore::OpenAIClient *>(c)
                                    || qobject_cast<::LLMQore::OpenAIResponsesClient *>(c)
                                    || qobject_cast<::LLMQore::LlamaCppClient *>(c);
    if (!isOpenAICompatible)
        return error;

    const QString baseUrl = c->url();
    const QString path = QUrl(baseUrl).path();
    const bool hasV1Segment = path.contains("/v1/") || path.endsWith("/v1");
    if (hasV1Segment)
        return error;

    return error
        + tr("\n\nHint: Your base URL (%1) does not contain a '/v1' path segment. "
             "Most OpenAI-compatible servers require it (e.g., %1/v1). "
             "Try updating the URL in settings.")
              .arg(baseUrl);
}

} // namespace QodeAssist::PluginLLMCore
