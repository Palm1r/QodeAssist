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

#include <LLMCore/BaseClient.hpp>
#include <LLMCore/ToolsManager.hpp>

#include <Logger.hpp>

namespace QodeAssist::PluginLLMCore {

Provider::Provider(QObject *parent)
    : QObject(parent)
{}

RequestID Provider::sendRequest(const QUrl &url, const QJsonObject &payload)
{
    auto *c = client();

    QUrl baseUrl(url);
    baseUrl.setPath("");
    c->setUrl(baseUrl.toString());
    c->setApiKey(apiKey());

    auto requestId = c->sendMessage(payload);

    LOG_MESSAGE(
        QString("%1: Sending request %2 to %3").arg(name(), requestId, url.toString()));

    return requestId;
}

void Provider::cancelRequest(const RequestID &requestId)
{
    LOG_MESSAGE(QString("%1: Cancelling request %2").arg(name(), requestId));
    client()->cancelRequest(requestId);
}

::LLMCore::ToolsManager *Provider::toolsManager() const
{
    return client()->tools();
}

} // namespace QodeAssist::PluginLLMCore
