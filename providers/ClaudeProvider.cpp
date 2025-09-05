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

ClaudeProvider::ClaudeProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsFactory(std::make_unique<Tools::ToolsFactory>())
    , m_toolHandler(std::make_unique<ClaudeToolHandler>(this))
{
    m_toolHandler->setToolsFactory(m_toolsFactory.get());

    connect(
        m_toolHandler.get(),
        &ClaudeToolHandler::toolResultReady,
        this,
        &ClaudeProvider::onToolResultReady);
}

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

bool ClaudeProvider::supportsTools() const
{
    return true;
}

LLMCore::IToolsFactory *ClaudeProvider::toolsFactory() const
{
    return m_toolsFactory.get();
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

    // Add tools if supported and in chat mode
    if (supportsTools() && m_toolsFactory && type == LLMCore::RequestType::Chat) {
        auto toolsDefinitions = m_toolsFactory->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Claude request")
                            .arg(m_toolsFactory->getAvailableTools().size()));
        }
    }
}

void ClaudeProvider::sendRequest(
    const QString &requestId, const QUrl &url, const QJsonObject &payload)
{
    // Initialize stream buffer and tool handler state
    m_streamBuffers[requestId] = QString();
    m_accumulatedResponses[requestId] = QString();
    m_toolHandler->initializeRequest(requestId, payload);

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(QString("ClaudeProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

void ClaudeProvider::onDataReceived(const QString &requestId, const QByteArray &data)
{
    m_streamBuffers[requestId] += QString::fromUtf8(data);

    QStringList lines = m_streamBuffers[requestId].split('\n');
    m_streamBuffers[requestId] = lines.takeLast();

    QString &accumulatedResponse = m_accumulatedResponses[requestId];
    QString tempResponse;
    bool isComplete = false;

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || !trimmedLine.startsWith("data:"))
            continue;

        QString jsonData = trimmedLine.mid(6).trimmed();
        if (jsonData == "[DONE]")
            continue;

        QJsonDocument jsonResponse = QJsonDocument::fromJson(jsonData.toUtf8());
        if (jsonResponse.isNull())
            continue;

        QJsonObject responseObj = jsonResponse.object();
        QString eventType = responseObj["type"].toString();

        if (eventType == "content_block_start") {
            QJsonObject contentBlock = responseObj["content_block"].toObject();
            if (contentBlock["type"].toString() == "tool_use") {
                QString toolName = contentBlock["name"].toString();
                QString toolId = contentBlock["id"].toString();
                QJsonObject input = contentBlock["input"].toObject();
                m_toolHandler->processToolStart(requestId, toolName, toolId, input);
            }
        } else if (eventType == "content_block_delta") {
            QJsonObject delta = responseObj["delta"].toObject();
            if (delta["type"].toString() == "text_delta") {
                QString text = delta["text"].toString();
                tempResponse += text;
                m_toolHandler->addAssistantText(requestId, text);
            }
        } else if (eventType == "message_delta") {
            if (responseObj.contains("delta")) {
                QJsonObject delta = responseObj["delta"].toObject();
                if (delta.contains("stop_reason")) {
                    QString stopReason = delta["stop_reason"].toString();
                    if (stopReason == "end_turn" && !m_toolHandler->hasActiveTool(requestId)) {
                        isComplete = true;
                    }
                }
            }
        } else if (eventType == "message_stop") {
            if (!m_toolHandler->hasActiveTool(requestId)) {
                isComplete = true;
            }
        }
    }

    if (!tempResponse.isEmpty()) {
        accumulatedResponse += tempResponse;
        emit partialResponseReceived(requestId, tempResponse);
    }

    if (isComplete) {
        emit fullResponseReceived(requestId, accumulatedResponse);
        m_accumulatedResponses.remove(requestId);
        m_streamBuffers.remove(requestId);
        m_toolHandler->clearRequest(requestId);
    }
}

void ClaudeProvider::onRequestFinished(const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("ClaudeProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
    } else {
        if (!m_toolHandler->hasActiveTool(requestId)) {
            const QString fullResponse = m_accumulatedResponses[requestId];
            if (!fullResponse.isEmpty()) {
                emit fullResponseReceived(requestId, fullResponse);
            }
        }
    }

    m_streamBuffers.remove(requestId);
    m_accumulatedResponses.remove(requestId);
    m_toolHandler->clearRequest(requestId);
}

void ClaudeProvider::onToolResultReady(const QString &requestId, const QJsonObject &newRequest)
{
    QNetworkRequest networkRequest(QUrl(url() + chatEndpoint()));
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = newRequest};

    LOG_MESSAGE(QString("Sending tool result continuation for request %1").arg(requestId));

    emit httpClient()->sendRequest(request);
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
        {"stream", {}},
        {"tools", QJsonArray{}}};

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

} // namespace QodeAssist::Providers
