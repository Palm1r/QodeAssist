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

#include "OllamaClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "llmcore/core/Log.hpp"

#include "OllamaMessage.hpp"

namespace QodeAssist::LLMCore {

OllamaClient::OllamaClient(QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &OllamaClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &OllamaClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &OllamaClient::handleToolExecutionComplete);
}

OllamaClient::OllamaClient(ApiKey apiKey, QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
    , m_apiKey(std::move(apiKey))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &OllamaClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &OllamaClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &OllamaClient::handleToolExecutionComplete);
}

void OllamaClient::setApiKey(const QString &apiKey)
{
    m_apiKey = [apiKey]() { return apiKey; };
}

QString OllamaClient::apiKey() const
{
    return m_apiKey ? m_apiKey() : QString{};
}

QNetworkRequest OllamaClient::prepareNetworkRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        request.setRawHeader("Authorization", "Basic " + apiKey().toLatin1());
    }

    return request;
}

QFuture<QList<QString>> OllamaClient::listModels(const QString &baseUrl)
{
    QUrl url(baseUrl + "/api/tags");
    QNetworkRequest request = prepareNetworkRequest(url);

    return m_httpClient->get(request)
        .then([](const QByteArray &data) {
            QList<QString> models;
            QJsonObject jsonObject = QJsonDocument::fromJson(data).object();
            QJsonArray modelArray = jsonObject["models"].toArray();

            for (const QJsonValue &value : modelArray) {
                QJsonObject modelObject = value.toObject();
                models.append(modelObject["name"].toString());
            }
            return models;
        })
        .onFailed([](const std::exception &e) {
            qCDebug(llmOllamaLog).noquote() << QString("Error fetching models: %1").arg(e.what());
            return QList<QString>{};
        });
}

void OllamaClient::sendMessage(
    const RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    if (!m_messages.contains(requestId)) {
        m_dataBuffers[requestId].clear();
    }

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    qCDebug(llmOllamaLog).noquote() << QString("Sending request %1 to %2").arg(requestId, url.toString());

    QNetworkRequest request = prepareNetworkRequest(url);
    m_httpClient->postStreaming(requestId, request, payload);
}

void OllamaClient::cancelRequest(const RequestID &requestId)
{
    qCDebug(llmOllamaLog).noquote() << QString("Cancelling request %1").arg(requestId);
    m_httpClient->cancelRequest(requestId);
    cleanupRequest(requestId);
}

void OllamaClient::handleDataReceived(const QString &requestId, const QByteArray &data)
{
    DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    if (data.isEmpty()) {
        return;
    }

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
        if (doc.isNull()) {
            qCDebug(llmOllamaLog).noquote() << QString("Failed to parse JSON: %1").arg(error.errorString());
            continue;
        }

        QJsonObject obj = doc.object();

        if (obj.contains("error") && !obj["error"].toString().isEmpty()) {
            qCDebug(llmOllamaLog).noquote() << "Error in response: " + obj["error"].toString();
            continue;
        }

        processStreamData(requestId, obj);
    }
}

void OllamaClient::handleRequestFinished(const QString &requestId, std::optional<QString> error)
{
    if (error) {
        qCDebug(llmOllamaLog).noquote() << QString("Request %1 failed: %2").arg(requestId, *error);
        emit requestFailed(requestId, *error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        OllamaMessage *message = m_messages[requestId];
        if (message->state() == MessageState::RequiresToolExecution) {
            qCDebug(llmOllamaLog).noquote() << QString("Waiting for tools to complete for %1").arg(requestId);
            return;
        }
    }

    QString finalText;
    if (m_messages.contains(requestId)) {
        OllamaMessage *message = m_messages[requestId];

        for (auto block : message->currentBlocks()) {
            if (auto textContent = qobject_cast<TextContent *>(block)) {
                finalText += textContent->text();
            }
        }

        if (!finalText.isEmpty()) {
            qCDebug(llmOllamaLog).noquote() << QString("Emitting full response for %1, length=%2")
                                                        .arg(requestId)
                                                        .arg(finalText.length());
            emit fullResponseReceived(requestId, finalText);
        }
    }

    cleanupRequest(requestId);
}

void OllamaClient::handleToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId)) {
        qCDebug(llmOllamaLog).noquote() << QString("ERROR: No message found for request %1").arg(requestId);
        cleanupRequest(requestId);
        return;
    }

    if (!m_requestUrls.contains(requestId) || !m_originalRequests.contains(requestId)) {
        qCDebug(llmOllamaLog).noquote() << QString("ERROR: Missing data for continuation request %1").arg(requestId);
        cleanupRequest(requestId);
        return;
    }

    qCDebug(llmOllamaLog).noquote() << QString("Tool execution complete for request %1").arg(requestId);

    OllamaMessage *message = m_messages[requestId];

    for (auto it = toolResults.begin(); it != toolResults.end(); ++it) {
        auto toolContent = message->getCurrentToolUseContent();
        for (auto tool : toolContent) {
            if (tool->id() == it.key()) {
                auto toolStringName = tools()->stringName(tool->name());
                emit toolExecutionCompleted(requestId, tool->id(), toolStringName, it.value());
                break;
            }
        }
    }

    QJsonObject continuationRequest = m_originalRequests[requestId];
    QJsonArray messages = continuationRequest["messages"].toArray();

    QJsonObject assistantMessage = message->toProviderFormat();
    messages.append(assistantMessage);

    qCDebug(llmOllamaLog).noquote() << QString("Assistant message with tool_calls:\n%1")
                                              .arg(QString::fromUtf8(
                                                  QJsonDocument(assistantMessage).toJson(QJsonDocument::Indented)));

    QJsonArray toolResultMessages = message->createToolResultMessages(toolResults);
    for (const auto &toolMsg : toolResultMessages) {
        messages.append(toolMsg);
        qCDebug(llmOllamaLog).noquote() << QString("Tool result message:\n%1")
                                                  .arg(QString::fromUtf8(
                                                      QJsonDocument(toolMsg.toObject()).toJson(QJsonDocument::Indented)));
    }

    continuationRequest["messages"] = messages;

    qCDebug(llmOllamaLog).noquote() << QString("Sending continuation request for %1 with %2 tool results")
                                              .arg(requestId)
                                              .arg(toolResults.size());

    sendMessage(requestId, m_requestUrls[requestId], continuationRequest);
}

void OllamaClient::processStreamData(const QString &requestId, const QJsonObject &data)
{
    OllamaMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new OllamaMessage(this);
        m_messages[requestId] = message;
        qCDebug(llmOllamaLog).noquote() << QString("Created NEW message for request %1").arg(requestId);

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
            qCDebug(llmOllamaLog).noquote() << QString("Starting continuation for request %1").arg(requestId);
        }
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
        qCDebug(llmOllamaLog).noquote() << QString("Cleared message state for continuation request %1").arg(requestId);
    }

    if (data.contains("thinking")) {
        QString thinkingDelta = data["thinking"].toString();
        if (!thinkingDelta.isEmpty()) {
            message->handleThinkingDelta(thinkingDelta);
            qCDebug(llmOllamaLog).noquote() << QString("Received thinking delta, length=%1")
                                                        .arg(thinkingDelta.length());
        }
    }

    if (data.contains("message")) {
        QJsonObject messageObj = data["message"].toObject();

        if (messageObj.contains("thinking")) {
            QString thinkingDelta = messageObj["thinking"].toString();
            if (!thinkingDelta.isEmpty()) {
                message->handleThinkingDelta(thinkingDelta);

                if (!m_thinkingStarted.contains(requestId)) {
                    auto thinkingBlocks = message->getCurrentThinkingContent();
                    if (!thinkingBlocks.isEmpty() && thinkingBlocks.first()) {
                        QString currentThinking = thinkingBlocks.first()->thinking();
                        QString displayThinking = currentThinking.length() > 50
                            ? QString("%1...").arg(currentThinking.left(50))
                            : currentThinking;

                        emit thinkingBlockReceived(requestId, displayThinking, "");
                        m_thinkingStarted.insert(requestId);
                    }
                }
            }
        }

        if (messageObj.contains("content")) {
            QString content = messageObj["content"].toString();
            if (!content.isEmpty()) {
                emitThinkingBlocks(requestId, message);

                message->handleContentDelta(content);

                bool hasTextContent = false;
                for (auto block : message->currentBlocks()) {
                    if (qobject_cast<TextContent *>(block)) {
                        hasTextContent = true;
                        break;
                    }
                }

                if (hasTextContent) {
                    DataBuffers &buffers = m_dataBuffers[requestId];
                    buffers.responseContent += content;
                    emit partialResponseReceived(requestId, content);
                }
            }
        }

        if (messageObj.contains("tool_calls")) {
            QJsonArray toolCalls = messageObj["tool_calls"].toArray();
            qCDebug(llmOllamaLog).noquote() << QString("Found %1 structured tool calls").arg(toolCalls.size());
            for (const auto &toolCallValue : toolCalls) {
                message->handleToolCall(toolCallValue.toObject());
            }
        }
    }
    else if (data.contains("response")) {
        QString content = data["response"].toString();
        if (!content.isEmpty()) {
            message->handleContentDelta(content);

            bool hasTextContent = false;
            for (auto block : message->currentBlocks()) {
                if (qobject_cast<TextContent *>(block)) {
                    hasTextContent = true;
                    break;
                }
            }

            if (hasTextContent) {
                DataBuffers &buffers = m_dataBuffers[requestId];
                buffers.responseContent += content;
                emit partialResponseReceived(requestId, content);
            }
        }
    }

    if (data["done"].toBool()) {
        if (data.contains("signature")) {
            QString signature = data["signature"].toString();
            message->handleThinkingComplete(signature);
            qCDebug(llmOllamaLog).noquote() << QString("Set thinking signature, length=%1")
                                                  .arg(signature.length());
        }

        message->handleDone(true);
        handleMessageComplete(requestId);
    }
}

void OllamaClient::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    OllamaMessage *message = m_messages[requestId];

    emitThinkingBlocks(requestId, message);

    if (message->state() == MessageState::RequiresToolExecution) {
        qCDebug(llmOllamaLog).noquote() << QString("Message requires tool execution for %1").arg(requestId);

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            qCDebug(llmOllamaLog).noquote() << QString("WARNING: No tools to execute for %1 despite RequiresToolExecution state")
                                                        .arg(requestId);
            return;
        }

        for (auto toolContent : toolUseContent) {
            auto toolStringName = tools()->stringName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);

            qCDebug(llmOllamaLog).noquote() << QString("Executing tool: name=%1, id=%2, input=%3")
                                                        .arg(toolContent->name())
                                                        .arg(toolContent->id())
                                                        .arg(QString::fromUtf8(
                                                            QJsonDocument(toolContent->input()).toJson(QJsonDocument::Compact)));

            tools()->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }

    } else {
        qCDebug(llmOllamaLog).noquote() << QString("Message marked as complete for %1").arg(requestId);
    }
}

void OllamaClient::cleanupRequest(const RequestID &requestId)
{
    qCDebug(llmOllamaLog).noquote() << QString("Cleaning up request %1").arg(requestId);

    if (m_messages.contains(requestId)) {
        auto msg = m_messages.take(requestId);
        msg->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_thinkingEmitted.remove(requestId);
    m_thinkingStarted.remove(requestId);

    tools()->cleanupRequest(requestId);
}

void OllamaClient::emitThinkingBlocks(const QString &requestId, OllamaMessage *message)
{
    if (!message || m_thinkingEmitted.contains(requestId)) {
        return;
    }

    auto thinkingBlocks = message->getCurrentThinkingContent();
    if (thinkingBlocks.isEmpty()) {
        return;
    }

    for (auto thinkingContent : thinkingBlocks) {
        emit thinkingBlockReceived(
            requestId, thinkingContent->thinking(), thinkingContent->signature());
        qCDebug(llmOllamaLog).noquote() << QString("Emitted thinking block for request %1, thinking length=%2, signature "
                                                    "length=%3")
                                                    .arg(requestId)
                                                    .arg(thinkingContent->thinking().length())
                                                    .arg(thinkingContent->signature().length());
    }
    m_thinkingEmitted.insert(requestId);
}

} // namespace QodeAssist::LLMCore
