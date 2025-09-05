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

#include "OpenAIProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "llmcore/OpenAIMessage.hpp"
#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

OpenAIProvider::OpenAIProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsFactory(std::make_unique<Tools::ToolsFactory>())
    , m_toolsManager(std::make_unique<OpenAIToolsManager>(this))
{
    m_toolsManager->setToolsFactory(m_toolsFactory.get());

    connect(
        m_toolsManager.get(),
        &OpenAIToolsManager::requestReadyForContinuation,
        this,
        &OpenAIProvider::onToolsRequestReadyForContinuation);
}

QString OpenAIProvider::name() const
{
    return "OpenAI";
}

QString OpenAIProvider::url() const
{
    return "https://api.openai.com";
}

QString OpenAIProvider::completionEndpoint() const
{
    return "/v1/chat/completions";
}

QString OpenAIProvider::chatEndpoint() const
{
    return "/v1/chat/completions";
}

bool OpenAIProvider::supportsModelListing() const
{
    return true;
}

bool OpenAIProvider::supportsTools() const
{
    return m_toolsManager->hasToolsSupport();
}

LLMCore::IToolsFactory *OpenAIProvider::toolsFactory() const
{
    return m_toolsFactory.get();
}

void OpenAIProvider::prepareRequest(
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

    if (supportsTools() && type == LLMCore::RequestType::Chat) {
        auto toolsDefinitions = m_toolsManager->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to OpenAI request").arg(toolsDefinitions.size()));
        }
    }
}

void OpenAIProvider::sendRequest(
    const QString &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_dataBuffers[requestId].clear();
    m_requestUrls[requestId] = url;

    // Инициализируем tools manager для этого запроса
    m_toolsManager->initializeRequest(requestId, payload);

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(QString("OpenAIProvider: Sending request %1 to %2").arg(requestId, url.toString()));
    emit httpClient()->sendRequest(request);
}

void OpenAIProvider::onDataReceived(const QString &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        QJsonObject event = parseEventLine(line);
        if (event.isEmpty())
            continue;

        // Передаем событие tools manager'у и получаем текст для отправки пользователю
        QString text = m_toolsManager->processEvent(requestId, event);

        if (!text.isEmpty()) {
            m_dataBuffers[requestId].responseContent += text;
            emit partialResponseReceived(requestId, text);
        }

        // Проверяем на завершение (для случаев без tools)
        if (event.contains("choices")) {
            QJsonArray choices = event["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                QString finishReason = choice["finish_reason"].toString();
                if (finishReason == "stop") {
                    emit fullResponseReceived(requestId, m_dataBuffers[requestId].responseContent);
                }
            }
        }
    }
}

void OpenAIProvider::onRequestFinished(const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("OpenAI request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
    } else {
        LOG_MESSAGE(QString("OpenAI request %1 completed successfully").arg(requestId));

        // Если нет активных tools, отправляем финальный ответ
        if (m_dataBuffers.contains(requestId)
            && !m_dataBuffers[requestId].responseContent.isEmpty()) {
            // Проверяем, что это не tool execution request
            bool hasToolsInProgress = false;
            // Простая проверка - если запрос содержал tools, то возможно есть pending execution
            // В реальности tools manager должен сам управлять состоянием

            if (!hasToolsInProgress) {
                emit fullResponseReceived(requestId, m_dataBuffers[requestId].responseContent);
            }
        }
    }

    cleanupRequest(requestId);
}

QJsonObject OpenAIProvider::parseEventLine(const QString &line)
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
        return QJsonObject();
    }

    return doc.object();
}

void OpenAIProvider::onToolsRequestReadyForContinuation(
    const QString &requestId, const QJsonObject &followUpRequest)
{
    LOG_MESSAGE(QString("Sending continuation request for %1").arg(requestId));
    sendRequest(requestId, m_requestUrls[requestId], followUpRequest);
}

void OpenAIProvider::cleanupRequest(const QString &requestId)
{
    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}

QList<QString> OpenAIProvider::getInstalledModels(const QString &baseUrl)
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
        LOG_MESSAGE(QString("Error fetching OpenAI models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> OpenAIProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
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

QString OpenAIProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

void OpenAIProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

LLMCore::ProviderID OpenAIProvider::providerID() const
{
    return LLMCore::ProviderID::OpenAI;
}

} // namespace QodeAssist::Providers
