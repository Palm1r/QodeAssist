/*
 * Copyright (C) 2026 Petr Mironychev
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

#include "OpenAIClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "llmcore/core/Log.hpp"

#include "OpenAIMessage.hpp"

#include <llmcore/network/SSEUtils.hpp>

namespace QodeAssist::LLMCore {

OpenAIClient::OpenAIClient(QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &OpenAIClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &OpenAIClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &OpenAIClient::handleToolExecutionComplete);
}

OpenAIClient::OpenAIClient(ApiKey apiKey, QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
    , m_apiKey(std::move(apiKey))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &OpenAIClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &OpenAIClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &OpenAIClient::handleToolExecutionComplete);
}

void OpenAIClient::setApiKey(const QString &apiKey)
{
    m_apiKey = [apiKey]() { return apiKey; };
}

QString OpenAIClient::apiKey() const
{
    return m_apiKey ? m_apiKey() : QString{};
}

void OpenAIClient::setAuthHeaderName(const QString &name)
{
    m_authHeaderName = name;
}

void OpenAIClient::setAuthHeaderPrefix(const QString &prefix)
{
    m_authHeaderPrefix = prefix;
}

QNetworkRequest OpenAIClient::prepareNetworkRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!m_authHeaderName.isEmpty() && !apiKey().isEmpty()) {
        request.setRawHeader(
            m_authHeaderName.toUtf8(),
            QString("%1%2").arg(m_authHeaderPrefix, apiKey()).toUtf8());
    }

    return request;
}

QFuture<QList<QString>> OpenAIClient::listModels(const QString &baseUrl)
{
    QUrl url(baseUrl + "/v1/models");
    QNetworkRequest request = prepareNetworkRequest(url);

    return m_httpClient->get(request)
        .then([](const QByteArray &data) {
            QList<QString> models;
            QJsonObject jsonObject = QJsonDocument::fromJson(data).object();

            if (jsonObject.contains("data")) {
                QJsonArray modelArray = jsonObject["data"].toArray();
                for (const QJsonValue &value : modelArray) {
                    QJsonObject modelObject = value.toObject();
                    if (modelObject.contains("id")) {
                        models.append(modelObject["id"].toString());
                    }
                }
            }
            return models;
        })
        .onFailed([](const std::exception &e) {
            qCDebug(llmOpenAILog).noquote() << QString("Error fetching models: %1").arg(e.what());
            return QList<QString>{};
        });
}

void OpenAIClient::sendMessage(
    const RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    if (!m_messages.contains(requestId)) {
        m_dataBuffers[requestId].clear();
    }

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    qCDebug(llmOpenAILog).noquote() << QString("Sending request %1 to %2").arg(requestId, url.toString());

    QNetworkRequest request = prepareNetworkRequest(url);
    m_httpClient->postStreaming(requestId, request, payload);
}

void OpenAIClient::cancelRequest(const RequestID &requestId)
{
    qCDebug(llmOpenAILog).noquote() << QString("Cancelling request %1").arg(requestId);
    m_httpClient->cancelRequest(requestId);
    cleanupRequest(requestId);
}

void OpenAIClient::handleDataReceived(const QString &requestId, const QByteArray &data)
{
    DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty() || line == "data: [DONE]") {
            continue;
        }

        QJsonObject chunk = SSEUtils::parseEventLine(line);
        if (chunk.isEmpty())
            continue;

        // Handle llama.cpp infill format (content + stop fields)
        if (chunk.contains("content") && !chunk.contains("choices")) {
            QString content = chunk["content"].toString();
            if (!content.isEmpty()) {
                buffers.responseContent += content;
                emit partialResponseReceived(requestId, content);
            }
            if (chunk["stop"].toBool()) {
                emit fullResponseReceived(requestId, buffers.responseContent);
                m_dataBuffers.remove(requestId);
            }
        } else if (chunk.contains("choices")) {
            processStreamChunk(requestId, chunk);
        }
    }
}

void OpenAIClient::handleRequestFinished(const QString &requestId, std::optional<QString> error)
{
    if (error) {
        qCDebug(llmOpenAILog).noquote() << QString("Request %1 failed: %2").arg(requestId, *error);
        emit requestFailed(requestId, *error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        OpenAIMessage *message = m_messages[requestId];
        if (message->state() == MessageState::RequiresToolExecution) {
            qCDebug(llmOpenAILog).noquote() << QString("Waiting for tools to complete for %1").arg(requestId);
            m_dataBuffers.remove(requestId);
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const DataBuffers &buffers = m_dataBuffers[requestId];
        if (!buffers.responseContent.isEmpty()) {
            qCDebug(llmOpenAILog).noquote() << QString("Emitting full response for %1").arg(requestId);
            emit fullResponseReceived(requestId, buffers.responseContent);
        }
    }

    cleanupRequest(requestId);
}

void OpenAIClient::handleToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        qCDebug(llmOpenAILog).noquote() << QString("ERROR: Missing data for continuation request %1").arg(requestId);
        cleanupRequest(requestId);
        return;
    }

    qCDebug(llmOpenAILog).noquote() << QString("Tool execution complete for request %1").arg(requestId);

    OpenAIMessage *message = m_messages[requestId];
    auto toolContent = message->getCurrentToolUseContent();
    for (auto it = toolResults.begin(); it != toolResults.end(); ++it) {
        for (auto tool : toolContent) {
            if (tool->id() == it.key()) {
                auto toolStringName = tools()->stringName(tool->name());
                emit toolExecutionCompleted(
                    requestId, tool->id(), toolStringName, toolResults[tool->id()]);
                break;
            }
        }
    }

    QJsonObject continuationRequest = m_originalRequests[requestId];
    QJsonArray messages = continuationRequest["messages"].toArray();

    messages.append(message->toProviderFormat());

    QJsonArray toolResultMessages = message->createToolResultMessages(toolResults);
    for (const auto &toolMsg : toolResultMessages) {
        messages.append(toolMsg);
    }

    continuationRequest["messages"] = messages;

    qCDebug(llmOpenAILog).noquote() << QString("Sending continuation request for %1 with %2 tool results")
                                           .arg(requestId)
                                           .arg(toolResults.size());

    sendMessage(requestId, m_requestUrls[requestId], continuationRequest);
}

void OpenAIClient::processStreamChunk(const QString &requestId, const QJsonObject &chunk)
{
    QJsonArray choices = chunk["choices"].toArray();
    if (choices.isEmpty()) {
        return;
    }

    QJsonObject choice = choices[0].toObject();
    QJsonObject delta = choice["delta"].toObject();
    QString finishReason = choice["finish_reason"].toString();

    OpenAIMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new OpenAIMessage(this);
        m_messages[requestId] = message;
        qCDebug(llmOpenAILog).noquote() << QString("Created NEW OpenAIMessage for request %1").arg(requestId);

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
            qCDebug(llmOpenAILog).noquote() << QString("Starting continuation for request %1").arg(requestId);
        }
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
        qCDebug(llmOpenAILog).noquote() << QString("Cleared message state for continuation request %1").arg(requestId);
    }

    if (delta.contains("content") && !delta["content"].isNull()) {
        QString content = delta["content"].toString();
        message->handleContentDelta(content);

        DataBuffers &buffers = m_dataBuffers[requestId];
        buffers.responseContent += content;
        emit partialResponseReceived(requestId, content);
    }

    if (delta.contains("tool_calls")) {
        QJsonArray toolCalls = delta["tool_calls"].toArray();
        for (const auto &toolCallValue : toolCalls) {
            QJsonObject toolCall = toolCallValue.toObject();
            int index = toolCall["index"].toInt();

            if (toolCall.contains("id")) {
                QString id = toolCall["id"].toString();
                QJsonObject function = toolCall["function"].toObject();
                QString name = function["name"].toString();
                message->handleToolCallStart(index, id, name);
            }

            if (toolCall.contains("function")) {
                QJsonObject function = toolCall["function"].toObject();
                if (function.contains("arguments")) {
                    QString args = function["arguments"].toString();
                    message->handleToolCallDelta(index, args);
                }
            }
        }
    }

    if (!finishReason.isEmpty() && finishReason != "null") {
        for (int i = 0; i < 10; ++i) {
            message->handleToolCallComplete(i);
        }

        message->handleFinishReason(finishReason);
        handleMessageComplete(requestId);
    }
}

void OpenAIClient::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    OpenAIMessage *message = m_messages[requestId];

    if (message->state() == MessageState::RequiresToolExecution) {
        qCDebug(llmOpenAILog).noquote() << QString("Message requires tool execution for %1").arg(requestId);

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            qCDebug(llmOpenAILog).noquote() << QString("No tools to execute for %1").arg(requestId);
            return;
        }

        for (auto toolContent : toolUseContent) {
            auto toolStringName = tools()->stringName(
                toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);
            tools()->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }

    } else {
        qCDebug(llmOpenAILog).noquote() << QString("Message marked as complete for %1").arg(requestId);
    }
}

void OpenAIClient::cleanupRequest(const RequestID &requestId)
{
    qCDebug(llmOpenAILog).noquote() << QString("Cleaning up request %1").arg(requestId);

    if (m_messages.contains(requestId)) {
        OpenAIMessage *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);

    tools()->cleanupRequest(requestId);
}

} // namespace QodeAssist::LLMCore
