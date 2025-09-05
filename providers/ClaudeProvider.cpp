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
        &ClaudeToolHandler::toolCompleted,
        this,
        &ClaudeProvider::onToolCompleted);
    connect(m_toolHandler.get(), &ClaudeToolHandler::toolFailed, this, &ClaudeProvider::onToolFailed);
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

    if (supportsTools() && m_toolsFactory && type == LLMCore::RequestType::Chat) {
        auto toolsDefinitions = m_toolsFactory->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Claude request")
                            .arg(m_toolsFactory->getAvailableTools().size()));
        }
    }
}

// ClaudeProvider.cpp - исправленный sendRequest
void ClaudeProvider::sendRequest(
    const QString &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_dataBuffers[requestId].clear();
    m_requestUrls[requestId] = url;

    // Простая инициализация состояния
    m_requestStates.insert(requestId, RequestState(payload));

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
    bool messageComplete = false;

    for (const QString &line : lines) {
        QJsonObject event = parseEventLine(line);
        if (event.isEmpty())
            continue;

        QString eventType = event["type"].toString();

        if (eventType == "content_block_start") {
            handleContentBlockStart(requestId, event);
        } else if (eventType == "content_block_delta") {
            tempResponse += handleContentBlockDelta(requestId, event);
        } else if (eventType == "content_block_stop") {
            handleContentBlockStop(requestId, event);
        } else if (eventType == "message_delta") {
            messageComplete = handleMessageDelta(requestId, event);
        }
    }

    updateResponseAndCheckCompletion(requestId, tempResponse, messageComplete);
}

void ClaudeProvider::onRequestFinished(const QString &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("Claude request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
    } else {
        LOG_MESSAGE(QString("Claude request %1 completed successfully").arg(requestId));

        // Отправляем финальный ответ если есть накопленный текст
        if (m_dataBuffers.contains(requestId)
            && !m_dataBuffers[requestId].responseContent.isEmpty()) {
            emit fullResponseReceived(requestId, m_dataBuffers[requestId].responseContent);
        }
    }

    cleanupRequest(requestId);
}

QJsonObject ClaudeProvider::parseEventLine(const QString &line)
{
    if (!line.startsWith("data: "))
        return QJsonObject();

    QString jsonStr = line.mid(6).trimmed();
    if (jsonStr == "[DONE]")
        return QJsonObject();

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    return doc.object();
}

void ClaudeProvider::handleContentBlockStart(const QString &requestId, const QJsonObject &event)
{
    if (!m_requestStates.contains(requestId))
        return;

    QJsonObject contentBlock = event["content_block"].toObject();
    if (contentBlock["type"].toString() == "tool_use") {
        RequestState &state = m_requestStates[requestId];
        state.currentToolId = contentBlock["id"].toString();
        state.currentToolName = contentBlock["name"].toString();
        state.accumulatedInput.clear();

        LOG_MESSAGE(QString("Tool use started: %1 (ID: %2)")
                        .arg(state.currentToolName, state.currentToolId));
    }
}

// ClaudeProvider.cpp
QString ClaudeProvider::handleContentBlockDelta(const QString &requestId, const QJsonObject &event)
{
    if (!m_requestStates.contains(requestId))
        return QString();

    QJsonObject delta = event["delta"].toObject();
    RequestState &state = m_requestStates[requestId];

    if (delta["type"].toString() == "text_delta") {
        QString text = delta["text"].toString();
        state.assistantText += text; // Просто накапливаем текст
        return text;
    } else if (delta["type"].toString() == "input_json_delta") {
        state.accumulatedInput += delta["partial_json"].toString();
    }

    return QString();
}

void ClaudeProvider::handleContentBlockStop(const QString &requestId, const QJsonObject &event)
{
    if (!m_requestStates.contains(requestId))
        return;

    RequestState &state = m_requestStates[requestId];
    if (!state.currentToolId.isEmpty()) {
        QJsonDocument inputDoc = QJsonDocument::fromJson(state.accumulatedInput.toUtf8());
        if (inputDoc.isNull()) {
            LOG_MESSAGE(QString("Invalid JSON input for tool %1").arg(state.currentToolName));
            return;
        }

        QJsonObject toolCall;
        toolCall["name"] = state.currentToolName;
        toolCall["input"] = inputDoc.object();

        state.toolCalls.append({state.currentToolId, toolCall});

        LOG_MESSAGE(QString("Executing tool %1").arg(state.currentToolName));
        executeTool(requestId, state.currentToolId, state.currentToolName, inputDoc.object());

        state.currentToolId.clear();
        state.currentToolName.clear();
    }
}

bool ClaudeProvider::handleMessageDelta(const QString &requestId, const QJsonObject &event)
{
    QJsonObject delta = event["delta"].toObject();
    return delta["stop_reason"].toString() == "tool_use"
           || delta["stop_reason"].toString() == "end_turn";
}

void ClaudeProvider::updateResponseAndCheckCompletion(
    const QString &requestId, const QString &tempResponse, bool messageComplete)
{
    if (!tempResponse.isEmpty()) {
        m_dataBuffers[requestId].responseContent += tempResponse;
        emit partialResponseReceived(requestId, tempResponse);
    }

    if (messageComplete) {
        if (!m_requestStates.contains(requestId) || m_requestStates[requestId].toolCalls.isEmpty()) {
            // Нет инструментов - завершаем ответ
            emit fullResponseReceived(requestId, m_dataBuffers[requestId].responseContent);
        }
        // Если есть инструменты, ждем их завершения в onToolCompleted/onToolFailed
    }
}

void ClaudeProvider::executeTool(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &input)
{
    m_toolHandler->executeTool(requestId, toolId, toolName, input);
}

void ClaudeProvider::onToolCompleted(
    const QString &requestId, const QString &toolId, const QString &result)
{
    if (!m_requestStates.contains(requestId)) {
        LOG_MESSAGE(QString("No request state found for completed tool %1").arg(toolId));
        return;
    }

    RequestState &state = m_requestStates[requestId];
    state.toolResults[toolId] = result;

    LOG_MESSAGE(QString("Tool %1 completed for request %2").arg(toolId, requestId));

    if (state.allToolsCompleted()) {
        // Создаем новый запрос с результатами всех инструментов
        QList<QPair<QString, QString>> toolResults;
        for (auto it = state.toolResults.begin(); it != state.toolResults.end(); ++it) {
            toolResults.append({it.key(), it.value()});
        }

        QJsonObject newRequest = ClaudeRequestMessage::createFollowUpRequest(
            state.originalRequest,
            state.originalMessages,
            state.assistantText,
            state.toolCalls,
            toolResults);

        LOG_MESSAGE(QString("Sending continuation request for %1").arg(requestId));
        sendRequest(requestId, m_requestUrls[requestId], newRequest);
    }
}

void ClaudeProvider::onToolFailed(
    const QString &requestId, const QString &toolId, const QString &error)
{
    if (!m_requestStates.contains(requestId)) {
        LOG_MESSAGE(QString("No request state found for failed tool %1").arg(toolId));
        return;
    }

    RequestState &state = m_requestStates[requestId];
    state.toolResults[toolId] = QString("Error: %1").arg(error);

    LOG_MESSAGE(QString("Tool %1 failed for request %2: %3").arg(toolId, requestId, error));

    if (state.allToolsCompleted()) {
        QList<QPair<QString, QString>> toolResults;
        for (auto it = state.toolResults.begin(); it != state.toolResults.end(); ++it) {
            toolResults.append({it.key(), it.value()});
        }

        QJsonObject newRequest = ClaudeRequestMessage::createFollowUpRequest(
            state.originalRequest,
            state.originalMessages,
            state.assistantText,
            state.toolCalls,
            toolResults);

        LOG_MESSAGE(
            QString("Sending continuation request after tool failure for %1").arg(requestId));
        sendRequest(requestId, m_requestUrls[requestId], newRequest);
    }
}

void ClaudeProvider::cleanupRequest(const QString &requestId)
{
    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_requestStates.remove(requestId);

    // Очищаем состояние инструментов
    m_toolHandler->cleanupRequest(requestId);
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
