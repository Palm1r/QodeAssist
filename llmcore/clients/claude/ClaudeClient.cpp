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

#include "ClaudeClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include "llmcore/core/Log.hpp"

#include "ClaudeMessage.hpp"

#include <llmcore/network/SSEUtils.hpp>

namespace QodeAssist::LLMCore {

ClaudeClient::ClaudeClient(QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &ClaudeClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &ClaudeClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &ClaudeClient::handleToolExecutionComplete);
}

ClaudeClient::ClaudeClient(ApiKey apiKey, QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
    , m_apiKey(std::move(apiKey))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &ClaudeClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &ClaudeClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &ClaudeClient::handleToolExecutionComplete);
}

QNetworkRequest ClaudeClient::prepareNetworkRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("anthropic-version", "2023-06-01");

    if (!apiKey().isEmpty()) {
        request.setRawHeader("x-api-key", apiKey().toUtf8());
    }

    return request;
}

void ClaudeClient::setApiKey(const QString &apiKey)
{
    m_apiKey = [apiKey]() { return apiKey; };
}

QString ClaudeClient::apiKey() const
{
    return m_apiKey ? m_apiKey() : QString{};
}

QFuture<QList<QString>> ClaudeClient::listModels(const QString &baseUrl)
{
    QUrl url(baseUrl + "/v1/models");
    QUrlQuery query;
    query.addQueryItem("limit", "1000");
    url.setQuery(query);

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
            qCDebug(llmClaudeLog).noquote() << QString("Error fetching models: %1").arg(e.what());
            return QList<QString>{};
        });
}

void ClaudeClient::sendMessage(
    const RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    if (!m_messages.contains(requestId)) {
        m_dataBuffers[requestId].clear();
    }

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    qCDebug(llmClaudeLog).noquote() << QString("Sending request %1 to %2").arg(requestId, url.toString());

    QNetworkRequest request = prepareNetworkRequest(url);
    m_httpClient->postStreaming(requestId, request, payload);
}

void ClaudeClient::cancelRequest(const RequestID &requestId)
{
    qCDebug(llmClaudeLog).noquote() << QString("Cancelling request %1").arg(requestId);
    m_httpClient->cancelRequest(requestId);
    cleanupRequest(requestId);
}

void ClaudeClient::handleDataReceived(const QString &requestId, const QByteArray &data)
{
    DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        QJsonObject responseObj = SSEUtils::parseEventLine(line);
        if (responseObj.isEmpty())
            continue;

        processStreamEvent(requestId, responseObj);
    }
}

void ClaudeClient::handleRequestFinished(const QString &requestId, std::optional<QString> error)
{
    if (error) {
        qCDebug(llmClaudeLog).noquote() << QString("Request %1 failed: %2").arg(requestId, *error);
        emit requestFailed(requestId, *error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        ClaudeMessage *message = m_messages[requestId];
        if (message->state() == MessageState::RequiresToolExecution) {
            qCDebug(llmClaudeLog).noquote() << QString("Waiting for tools to complete for %1").arg(requestId);
            m_dataBuffers.remove(requestId);
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const DataBuffers &buffers = m_dataBuffers[requestId];
        if (!buffers.responseContent.isEmpty()) {
            qCDebug(llmClaudeLog).noquote() << QString("Emitting full response for %1").arg(requestId);
            emit fullResponseReceived(requestId, buffers.responseContent);
        }
    }

    cleanupRequest(requestId);
}

void ClaudeClient::handleToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        qCDebug(llmClaudeLog).noquote() << QString("ERROR: Missing data for continuation request %1").arg(requestId);
        cleanupRequest(requestId);
        return;
    }

    qCDebug(llmClaudeLog).noquote() << QString("Tool execution complete for request %1").arg(requestId);

    ClaudeMessage *message = m_messages[requestId];
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

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = message->createToolResultsContent(toolResults);
    messages.append(userMessage);

    continuationRequest["messages"] = messages;

    if (continuationRequest.contains("thinking")) {
        QJsonObject thinkingObj = continuationRequest["thinking"].toObject();
        qCDebug(llmClaudeLog).noquote() << QString("Thinking mode preserved for continuation: type=%1, budget=%2 tokens")
                        .arg(thinkingObj["type"].toString())
                        .arg(thinkingObj["budget_tokens"].toInt());
    }

    qCDebug(llmClaudeLog).noquote() << QString("Sending continuation request for %1 with %2 tool results")
                    .arg(requestId)
                    .arg(toolResults.size());

    sendMessage(requestId, m_requestUrls[requestId], continuationRequest);
}

void ClaudeClient::processStreamEvent(const QString &requestId, const QJsonObject &event)
{
    QString eventType = event["type"].toString();

    if (eventType == "message_stop") {
        return;
    }

    ClaudeMessage *message = m_messages.value(requestId);
    if (!message) {
        if (eventType == "message_start") {
            message = new ClaudeMessage(this);
            m_messages[requestId] = message;
            qCDebug(llmClaudeLog).noquote() << QString("Created NEW ClaudeMessage for request %1").arg(requestId);
        } else {
            return;
        }
    }

    if (eventType == "message_start") {
        message->startNewContinuation();
        emit continuationStarted(requestId);
        qCDebug(llmClaudeLog).noquote() << QString("Starting NEW continuation for request %1").arg(requestId);

    } else if (eventType == "content_block_start") {
        int index = event["index"].toInt();
        QJsonObject contentBlock = event["content_block"].toObject();
        QString blockType = contentBlock["type"].toString();

        qCDebug(llmClaudeLog).noquote()
            << QString("Adding new content block: type=%1, index=%2").arg(blockType).arg(index);

        if (blockType == "thinking" || blockType == "redacted_thinking") {
            QJsonDocument eventDoc(event);
            qCDebug(llmClaudeLog).noquote() << QString("content_block_start event for %1: %2")
                            .arg(blockType)
                            .arg(QString::fromUtf8(eventDoc.toJson(QJsonDocument::Compact)));
        }

        message->handleContentBlockStart(index, blockType, contentBlock);

    } else if (eventType == "content_block_delta") {
        int index = event["index"].toInt();
        QJsonObject delta = event["delta"].toObject();
        QString deltaType = delta["type"].toString();

        message->handleContentBlockDelta(index, deltaType, delta);

        if (deltaType == "text_delta") {
            QString text = delta["text"].toString();
            DataBuffers &buffers = m_dataBuffers[requestId];
            buffers.responseContent += text;
            emit partialResponseReceived(requestId, text);
        } else if (deltaType == "signature_delta") {
            QString signature = delta["signature"].toString();
            Q_UNUSED(signature)
        }

    } else if (eventType == "content_block_stop") {
        int index = event["index"].toInt();

        auto allBlocks = message->getCurrentBlocks();
        if (index < allBlocks.size()) {
            QString blockType = allBlocks[index]->type();
            if (blockType == "thinking" || blockType == "redacted_thinking") {
                QJsonDocument eventDoc(event);
                qCDebug(llmClaudeLog).noquote() << QString("content_block_stop event for %1 at index %2: %3")
                                .arg(blockType)
                                .arg(index)
                                .arg(QString::fromUtf8(eventDoc.toJson(QJsonDocument::Compact)));
            }
        }

        if (event.contains("content_block")) {
            QJsonObject contentBlock = event["content_block"].toObject();
            QString blockType = contentBlock["type"].toString();

            if (blockType == "thinking") {
                QString signature = contentBlock["signature"].toString();
                if (!signature.isEmpty()) {
                    auto allBlocks = message->getCurrentBlocks();
                    if (index < allBlocks.size()) {
                        if (auto thinkingContent
                            = qobject_cast<ThinkingContent *>(allBlocks[index])) {
                            thinkingContent->setSignature(signature);
                            qCDebug(llmClaudeLog).noquote()
                                << QString("Updated thinking block signature from "
                                           "content_block_stop, signature length=%1")
                                       .arg(signature.length());
                        }
                    }
                }
            } else if (blockType == "redacted_thinking") {
                QString signature = contentBlock["signature"].toString();
                if (!signature.isEmpty()) {
                    auto allBlocks = message->getCurrentBlocks();
                    if (index < allBlocks.size()) {
                        if (auto redactedContent
                            = qobject_cast<RedactedThinkingContent *>(allBlocks[index])) {
                            redactedContent->setSignature(signature);
                            qCDebug(llmClaudeLog).noquote()
                                << QString("Updated redacted_thinking block signature from "
                                           "content_block_stop, signature length=%1")
                                       .arg(signature.length());
                        }
                    }
                }
            }
        }

        message->handleContentBlockStop(index);

        auto thinkingBlocks = message->getCurrentThinkingContent();
        for (auto thinkingContent : thinkingBlocks) {
            auto allBlocks = message->getCurrentBlocks();
            if (index < allBlocks.size() && allBlocks[index] == thinkingContent) {
                emit thinkingBlockReceived(
                    requestId, thinkingContent->thinking(), thinkingContent->signature());
                qCDebug(llmClaudeLog).noquote()
                    << QString(
                           "Emitted thinking block for request %1, thinking length=%2, "
                           "signature length=%3")
                           .arg(requestId)
                           .arg(thinkingContent->thinking().length())
                           .arg(thinkingContent->signature().length());
                break;
            }
        }

        auto redactedBlocks = message->getCurrentRedactedThinkingContent();
        for (auto redactedContent : redactedBlocks) {
            auto allBlocks = message->getCurrentBlocks();
            if (index < allBlocks.size() && allBlocks[index] == redactedContent) {
                emit redactedThinkingBlockReceived(requestId, redactedContent->signature());
                qCDebug(llmClaudeLog).noquote()
                    << QString("Emitted redacted thinking block for request %1, signature length=%2")
                           .arg(requestId)
                           .arg(redactedContent->signature().length());
                break;
            }
        }

    } else if (eventType == "message_delta") {
        QJsonObject delta = event["delta"].toObject();
        if (delta.contains("stop_reason")) {
            QString stopReason = delta["stop_reason"].toString();
            message->handleStopReason(stopReason);
            handleMessageComplete(requestId);
        }
    }
}

void ClaudeClient::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    ClaudeMessage *message = m_messages[requestId];

    if (message->state() == MessageState::RequiresToolExecution) {
        qCDebug(llmClaudeLog).noquote() << QString("Message requires tool execution for %1").arg(requestId);

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            qCDebug(llmClaudeLog).noquote() << QString("No tools to execute for %1").arg(requestId);
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
        qCDebug(llmClaudeLog).noquote() << QString("Message marked as complete for %1").arg(requestId);
    }
}

void ClaudeClient::cleanupRequest(const RequestID &requestId)
{
    qCDebug(llmClaudeLog).noquote() << QString("Cleaning up request %1").arg(requestId);

    if (m_messages.contains(requestId)) {
        ClaudeMessage *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);

    tools()->cleanupRequest(requestId);
}

} // namespace QodeAssist::LLMCore
