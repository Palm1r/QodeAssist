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

#include "LMStudioProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

LMStudioProvider::LMStudioProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsFactory(std::make_unique<Tools::ToolsFactory>())
    , m_toolsManager(std::make_unique<OpenAIToolsManager>(this))
{
    m_toolsManager->setToolsFactory(m_toolsFactory.get());

    connect(
        m_toolsManager.get(),
        &OpenAIToolsManager::requestReadyForContinuation,
        this,
        &LMStudioProvider::onToolsRequestReadyForContinuation);
}

QString LMStudioProvider::name() const
{
    return "LMStudio";
}

QString LMStudioProvider::url() const
{
    return "http://localhost:1234";
}

QString LMStudioProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

QString LMStudioProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool LMStudioProvider::supportsModelListing() const
{
    return true;
}

bool LMStudioProvider::supportsTools() const
{
    // For testing purposes, assume all LMStudio models support tools
    return m_toolsManager->hasToolsSupport();
}

LLMCore::IToolsFactory *LMStudioProvider::toolsFactory() const
{
    return m_toolsFactory.get();
}

void LMStudioProvider::prepareRequest(
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
        if (settings.useFrequencyPenalty())
            request["frequency_penalty"] = settings.frequencyPenalty();
        if (settings.usePresencePenalty())
            request["presence_penalty"] = settings.presencePenalty();

        request["stream"] = true;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else {
        applyModelParams(Settings::chatAssistantSettings());
    }

    // Add tools for chat requests (assuming all models support them for testing)
    if (supportsTools() && type == LLMCore::RequestType::Chat) {
        auto toolsDefinitions = m_toolsManager->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to LMStudio request").arg(toolsDefinitions.size()));
        }
    }
}

void LMStudioProvider::sendRequest(
    const QString &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_dataBuffers[requestId].clear();
    m_requestUrls[requestId] = url;

    // Initialize tools manager for this request
    m_toolsManager->initializeRequest(requestId, payload);

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(
        QString("LMStudioProvider: Sending request %1 to %2").arg(requestId, url.toString()));
    emit httpClient()->sendRequest(request);
}

void LMStudioProvider::onDataReceived(const QString &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        QJsonObject event = parseEventLine(line);
        if (event.isEmpty())
            continue;

        // Process event through tools manager and get text for user
        QString text = m_toolsManager->processEvent(requestId, event);

        if (!text.isEmpty()) {
            m_dataBuffers[requestId].responseContent += text;
            emit partialResponseReceived(requestId, text);
        }

        // Check for completion (for cases without tools)
        if (event.contains("choices")) {
            QJsonArray choices = event["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                QString finishReason = choice["finish_reason"].toString();
                if (finishReason == "stop") {
                    // Only send final response if no tools are active
                    if (!m_toolsManager->hasActiveTools(requestId)) {
                        emit fullResponseReceived(requestId, m_dataBuffers[requestId].responseContent);
                    }
                }
            }
        }
    }
}

void LMStudioProvider::onRequestFinished(const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("LMStudio request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
    } else {
        LOG_MESSAGE(QString("LMStudio request %1 completed successfully").arg(requestId));

        // Check if there are active tools being processed
        if (!m_toolsManager->hasActiveTools(requestId)) {
            // No tools in progress - send final response if we have content
            if (m_dataBuffers.contains(requestId)
                && !m_dataBuffers[requestId].responseContent.isEmpty()) {
                emit fullResponseReceived(requestId, m_dataBuffers[requestId].responseContent);
            }
        }
        // If tools are active, they will handle continuation via onToolsRequestReadyForContinuation
    }

    cleanupRequest(requestId);
}

QJsonObject LMStudioProvider::parseEventLine(const QString &line)
{
    if (line.trimmed().isEmpty() || line == "data: [DONE]") {
        return QJsonObject();
    }

    QString jsonStr = line;
    if (line.startsWith("data: ")) {
        jsonStr = line.mid(6).trimmed();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);

    if (doc.isNull()) {
        if (error.error != QJsonParseError::NoError) {
            LOG_MESSAGE(QString("LMStudio JSON parse error: %1").arg(error.errorString()));
        }
        return QJsonObject();
    }

    return doc.object();
}

void LMStudioProvider::onToolsRequestReadyForContinuation(
    const QString &requestId, const QJsonObject &followUpRequest)
{
    LOG_MESSAGE(QString("LMStudio: Sending continuation request for %1").arg(requestId));
    sendRequest(requestId, m_requestUrls[requestId], followUpRequest);
}

void LMStudioProvider::cleanupRequest(const QString &requestId)
{
    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}

QList<QString> LMStudioProvider::getInstalledModels(const QString &baseUrl)
{
    QList<QString> models;
    QNetworkAccessManager manager;

    QUrl url(baseUrl + "/v1/models");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
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
        LOG_MESSAGE(QString("Error fetching models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> LMStudioProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto templateReq = QJsonObject{
        {"model", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"temperature", {}},
        {"max_tokens", {}},
        {"top_p", {}},
        {"top_k", {}},
        {"frequency_penalty", {}},
        {"presence_penalty", {}},
        {"stop", QJsonArray{}},
        {"stream", {}},
        {"tools", QJsonArray{}}};

    return LLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString LMStudioProvider::apiKey() const
{
    return QString();
}

void LMStudioProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

LLMCore::ProviderID LMStudioProvider::providerID() const
{
    return LLMCore::ProviderID::LMStudio;
}

} // namespace QodeAssist::Providers
