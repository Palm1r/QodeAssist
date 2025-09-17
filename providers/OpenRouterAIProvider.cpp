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

#include "OpenRouterAIProvider.hpp"

#include "settings/ProviderSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "llmcore/OpenAIMessage.hpp"
#include "logger/Logger.hpp"

namespace QodeAssist::Providers {

QString OpenRouterProvider::name() const
{
    return "OpenRouter";
}

QString OpenRouterProvider::url() const
{
    return "https://openrouter.ai/api";
}

QString OpenRouterProvider::apiKey() const
{
    return Settings::providerSettings().openRouterApiKey();
}

LLMCore::ProviderID OpenRouterProvider::providerID() const
{
    return LLMCore::ProviderID::OpenRouter;
}

void OpenRouterProvider::onDataReceived(const QString &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    if (data.isEmpty()) {
        return;
    }

    bool isDone = false;
    QString tempResponse;

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty() || line.contains("OPENROUTER PROCESSING")) {
            continue;
        }

        if (line == "data: [DONE]") {
            isDone = true;
            continue;
        }

        QJsonObject responseObj = parseEventLine(line);
        if (responseObj.isEmpty())
            continue;

        auto message = LLMCore::OpenAIMessage::fromJson(responseObj);
        if (message.hasError()) {
            LOG_MESSAGE("Error in OpenRouter response: " + message.error);
            continue;
        }

        QString content = message.getContent();
        if (!content.isEmpty()) {
            tempResponse += content;
        }

        if (message.isDone()) {
            isDone = true;
        }
    }

    if (!tempResponse.isEmpty()) {
        buffers.responseContent += tempResponse;
        emit partialResponseReceived(requestId, tempResponse);
    }

    if (isDone) {
        emit fullResponseReceived(requestId, buffers.responseContent);
        m_dataBuffers.remove(requestId);
    }
}

void OpenRouterProvider::onRequestFinished(
    const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("OpenRouterProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
    } else {
        if (m_dataBuffers.contains(requestId)) {
            const LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
            if (!buffers.responseContent.isEmpty()) {
                emit fullResponseReceived(requestId, buffers.responseContent);
            }
        }
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
}

} // namespace QodeAssist::Providers
