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

#include "ClaudeProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

QString ClaudeProvider::name() const
{
    return "Claude";
}

QString ClaudeProvider::url() const
{
    return "https://api.anthropic.com";
}

QString ClaudeProvider::completionEndpoint() const
{
    return "/v1/messages";
}

QString ClaudeProvider::chatEndpoint() const
{
    return "/v1/messages";
}

bool ClaudeProvider::supportsModelListing() const
{
    return true;
}

void ClaudeProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    auto applyModelParams = [&request](const auto &settings) {
        request["max_tokens"] = settings.maxTokens();
        request["temperature"] = settings.temperature();
        if (settings.useTopP())
            request["top_p"] = settings.topP();
        if (settings.useTopK())
            request["top_k"] = settings.topK();
        request["stream"] = true;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }
}

QList<QString> ClaudeProvider::getInstalledModels(const QString &baseUrl)
{
    QList<QString> models;
    QNetworkAccessManager manager;

    QUrl url(baseUrl + "/v1/models");
    QUrlQuery query;
    query.addQueryItem("limit", "1000");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("anthropic-version", "2023-06-01");

    if (!apiKey().isEmpty()) {
        request.setRawHeader("x-api-key", apiKey().toUtf8());
    }

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains("data")) {
            QJsonArray modelArray = jsonObject["data"].toArray();
            for (const QJsonValue &value : modelArray) {
                QJsonObject modelObject = value.toObject();
                if (modelObject.contains("id")) {
                    QString modelId = modelObject["id"].toString();
                    models.append(modelId);
                }
            }
        }
    } else {
        LOG_MESSAGE(QString("Error fetching Claude models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> ClaudeProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto templateReq = QJsonObject{
        {"model", {}},
        {"system", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"temperature", {}},
        {"max_tokens", {}},
        {"anthropic-version", {}},
        {"top_p", {}},
        {"top_k", {}},
        {"stop", QJsonArray{}},
        {"stream", {}}};

    return LLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString ClaudeProvider::apiKey() const
{
    return Settings::providerSettings().claudeApiKey();
}

void ClaudeProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("anthropic-version", "2023-06-01");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("x-api-key", apiKey().toUtf8());
    }
}

LLMCore::ProviderID ClaudeProvider::providerID() const
{
    return LLMCore::ProviderID::Claude;
}

void ClaudeProvider::sendRequest(
    const QString &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_dataBuffers[requestId].clear();
    m_requestUrls[requestId] = url;

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(QString("ClaudeProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

void ClaudeProvider::onDataReceived(const QString &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    QString tempResponse;
    bool isComplete = false;

    for (const QString &line : lines) {
        QJsonObject responseObj = parseEventLine(line);
        if (responseObj.isEmpty())
            continue;

        QString eventType = responseObj["type"].toString();

        if (eventType == "message_start") {
            QString messageId = responseObj["message"].toObject()["id"].toString();
            LOG_MESSAGE(QString("Claude message started: %1").arg(messageId));

        } else if (eventType == "content_block_delta") {
            QJsonObject delta = responseObj["delta"].toObject();
            if (delta["type"].toString() == "text_delta") {
                tempResponse += delta["text"].toString();
            }

        } else if (eventType == "message_delta") {
            QJsonObject delta = responseObj["delta"].toObject();
            if (delta.contains("stop_reason")) {
                isComplete = true;
                QJsonObject usage = responseObj["usage"].toObject();
                LOG_MESSAGE(QString("Tokens: input=%1, output=%2")
                                .arg(usage["input_tokens"].toInt())
                                .arg(usage["output_tokens"].toInt()));
            }
        }
    }

    if (!tempResponse.isEmpty()) {
        buffers.responseContent += tempResponse;
        emit partialResponseReceived(requestId, tempResponse);
    }

    if (isComplete) {
        emit fullResponseReceived(requestId, buffers.responseContent);
        m_dataBuffers.remove(requestId);
    }
}

void ClaudeProvider::onRequestFinished(const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("ClaudeProvider request %1 failed: %2").arg(requestId, error));
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
