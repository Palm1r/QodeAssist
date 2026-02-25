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

#include "OpenAIResponsesClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <Logger.hpp>

#include "OpenAIResponsesMessage.hpp"

namespace QodeAssist::LLMCore {

OpenAIResponsesClient::OpenAIResponsesClient(QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
{
    connect(
        m_httpClient, &HttpClient::dataReceived, this, &OpenAIResponsesClient::handleDataReceived);
    connect(
        m_httpClient,
        &HttpClient::requestFinished,
        this,
        &OpenAIResponsesClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &OpenAIResponsesClient::handleToolExecutionComplete);
}

OpenAIResponsesClient::OpenAIResponsesClient(ApiKey apiKey, QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
    , m_apiKey(std::move(apiKey))
{
    connect(
        m_httpClient, &HttpClient::dataReceived, this, &OpenAIResponsesClient::handleDataReceived);
    connect(
        m_httpClient,
        &HttpClient::requestFinished,
        this,
        &OpenAIResponsesClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &OpenAIResponsesClient::handleToolExecutionComplete);
}

void OpenAIResponsesClient::setApiKey(const QString &apiKey)
{
    m_apiKey = [apiKey]() { return apiKey; };
}

QString OpenAIResponsesClient::apiKey() const
{
    return m_apiKey ? m_apiKey() : QString{};
}

QNetworkRequest OpenAIResponsesClient::prepareNetworkRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        request.setRawHeader(
            "Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }

    return request;
}

QFuture<QList<QString>> OpenAIResponsesClient::listModels(const QString &baseUrl)
{
    QUrl url(QString("%1/v1/models").arg(baseUrl));
    QNetworkRequest request = prepareNetworkRequest(url);

    return m_httpClient->get(request)
        .then([](const QByteArray &data) {
            QList<QString> models;
            const QJsonObject jsonObject = QJsonDocument::fromJson(data).object();

            if (jsonObject.contains("data")) {
                const QJsonArray modelArray = jsonObject["data"].toArray();
                models.reserve(modelArray.size());

                static const QStringList modelPrefixes = {"gpt-5", "o1", "o2", "o3", "o4"};

                for (const QJsonValue &value : modelArray) {
                    const QJsonObject modelObject = value.toObject();
                    if (!modelObject.contains("id")) {
                        continue;
                    }

                    const QString modelId = modelObject["id"].toString();
                    for (const QString &prefix : modelPrefixes) {
                        if (modelId.contains(prefix)) {
                            models.append(modelId);
                            break;
                        }
                    }
                }
            }
            return models;
        })
        .onFailed([](const std::exception &e) {
            LOG_MESSAGE(QString("Error fetching OpenAI models: %1").arg(e.what()));
            return QList<QString>{};
        });
}

void OpenAIResponsesClient::sendMessage(
    const RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    if (!m_messages.contains(requestId)) {
        m_dataBuffers[requestId].clear();
    }

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    LOG_MESSAGE(
        QString("OpenAIResponsesClient: Sending request %1 to %2").arg(requestId, url.toString()));

    QNetworkRequest request = prepareNetworkRequest(url);
    m_httpClient->postStreaming(requestId, request, payload);
}

void OpenAIResponsesClient::cancelRequest(const RequestID &requestId)
{
    LOG_MESSAGE(QString("OpenAIResponsesClient: Cancelling request %1").arg(requestId));
    m_httpClient->cancelRequest(requestId);
    cleanupRequest(requestId);
}

void OpenAIResponsesClient::handleDataReceived(const QString &requestId, const QByteArray &data)
{
    DataBuffers &buffers = m_dataBuffers[requestId];
    const QStringList lines = buffers.rawStreamBuffer.processData(data);

    QString currentEventType;

    for (const QString &line : lines) {
        const QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }

        if (line == "data: [DONE]") {
            continue;
        }

        if (line.startsWith("event: ")) {
            currentEventType = line.mid(7).trimmed();
            continue;
        }

        QString dataLine = line;
        if (line.startsWith("data: ")) {
            dataLine = line.mid(6);
        }

        const QJsonDocument doc = QJsonDocument::fromJson(dataLine.toUtf8());
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            processStreamEvent(requestId, currentEventType, obj);
        }
    }
}

void OpenAIResponsesClient::handleRequestFinished(
    const QString &requestId, std::optional<QString> error)
{
    if (error) {
        LOG_MESSAGE(
            QString("OpenAIResponsesClient request %1 failed: %2").arg(requestId, *error));
        emit requestFailed(requestId, *error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        OpenAIResponsesMessage *message = m_messages[requestId];
        if (message->state() == MessageState::RequiresToolExecution) {
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const DataBuffers &buffers = m_dataBuffers[requestId];
        if (!buffers.responseContent.isEmpty()) {
            emit fullResponseReceived(requestId, buffers.responseContent);
        } else {
            LOG_MESSAGE(
                QString("WARNING: OpenAIResponses - Response content is empty for %1, "
                        "emitting empty response")
                    .arg(requestId));
            emit fullResponseReceived(requestId, "");
        }
    } else {
        LOG_MESSAGE(
            QString("WARNING: OpenAIResponses - No data buffer found for %1").arg(requestId));
    }

    cleanupRequest(requestId);
}

void OpenAIResponsesClient::handleToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        LOG_MESSAGE(
            QString("ERROR: OpenAIResponses - Missing data for continuation request %1")
                .arg(requestId));
        cleanupRequest(requestId);
        return;
    }

    OpenAIResponsesMessage *message = m_messages[requestId];
    const auto toolContent = message->getCurrentToolUseContent();

    for (auto it = toolResults.constBegin(); it != toolResults.constEnd(); ++it) {
        for (const auto *tool : toolContent) {
            if (tool->id() == it.key()) {
                const auto toolStringName
                    = tools()->stringName(tool->name());
                emit toolExecutionCompleted(
                    requestId, tool->id(), toolStringName, toolResults[tool->id()]);
                break;
            }
        }
    }

    QJsonObject continuationRequest = m_originalRequests[requestId];
    QJsonArray input = continuationRequest["input"].toArray();

    const QList<QJsonObject> assistantItems = message->toItemsFormat();
    for (const QJsonObject &item : assistantItems) {
        input.append(item);
    }

    const QJsonArray toolResultItems = message->createToolResultItems(toolResults);
    for (const QJsonValue &item : toolResultItems) {
        input.append(item);
    }

    continuationRequest["input"] = input;

    m_dataBuffers[requestId].responseContent.clear();

    sendMessage(requestId, m_requestUrls[requestId], continuationRequest);
}

void OpenAIResponsesClient::processStreamEvent(
    const QString &requestId, const QString &eventType, const QJsonObject &data)
{
    OpenAIResponsesMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new OpenAIResponsesMessage(this);
        m_messages[requestId] = message;

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
        }
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
    }

    if (eventType == "response.content_part.added") {
    } else if (eventType == "response.output_text.delta") {
        const QString delta = data["delta"].toString();
        if (!delta.isEmpty()) {
            message->handleContentDelta(delta);
            m_dataBuffers[requestId].responseContent += delta;
            emit partialResponseReceived(requestId, delta);
        }
    } else if (eventType == "response.output_text.done") {
        const QString fullText = data["text"].toString();
        if (!fullText.isEmpty()) {
            m_dataBuffers[requestId].responseContent = fullText;
        }
    } else if (eventType == "response.content_part.done") {
    } else if (eventType == "response.output_item.added") {
        const QJsonObject item = data["item"].toObject();
        const QString itemType = item["type"].toString();

        if (itemType == "function_call") {
            const QString callId = item["call_id"].toString();
            const QString name = item["name"].toString();
            const QString itemId = item["id"].toString();

            if (!callId.isEmpty() && !name.isEmpty()) {
                if (!m_itemIdToCallId.contains(requestId)) {
                    m_itemIdToCallId[requestId] = QHash<QString, QString>();
                }
                m_itemIdToCallId[requestId][itemId] = callId;
                message->handleToolCallStart(callId, name);
            }
        } else if (itemType == "reasoning") {
            const QString itemId = item["id"].toString();
            if (!itemId.isEmpty()) {
                message->handleReasoningStart(itemId);
            }
        }
    } else if (eventType == "response.reasoning_content.delta") {
        const QString itemId = data["item_id"].toString();
        const QString delta = data["delta"].toString();
        if (!itemId.isEmpty() && !delta.isEmpty()) {
            message->handleReasoningDelta(itemId, delta);
        }
    } else if (eventType == "response.reasoning_content.done") {
        const QString itemId = data["item_id"].toString();
        if (!itemId.isEmpty()) {
            message->handleReasoningComplete(itemId);
            emitPendingThinkingBlocks(requestId);
        }
    } else if (eventType == "response.function_call_arguments.delta") {
        const QString itemId = data["item_id"].toString();
        const QString delta = data["delta"].toString();
        if (!itemId.isEmpty() && !delta.isEmpty()) {
            const QString callId = m_itemIdToCallId.value(requestId).value(itemId);
            if (!callId.isEmpty()) {
                message->handleToolCallDelta(callId, delta);
            } else {
                LOG_MESSAGE(
                    QString("ERROR: No call_id mapping found for item_id: %1").arg(itemId));
            }
        }
    } else if (
        eventType == "response.function_call_arguments.done"
        || eventType == "response.output_item.done") {
        const QString itemId = data["item_id"].toString();
        const QJsonObject item = data["item"].toObject();

        if (!item.isEmpty() && item["type"].toString() == "reasoning") {
            const QString finalItemId = itemId.isEmpty() ? item["id"].toString() : itemId;
            QString reasoningText = extractReasoningText(item);

            if (reasoningText.isEmpty()) {
                reasoningText = QString(
                    "[Reasoning process completed, but detailed thinking is not available in "
                    "streaming mode. The model has processed your request with extended "
                    "reasoning.]");
            }

            if (!finalItemId.isEmpty()) {
                message->handleReasoningDelta(finalItemId, reasoningText);
                message->handleReasoningComplete(finalItemId);
                emitPendingThinkingBlocks(requestId);
            }
        } else if (item.isEmpty() && !itemId.isEmpty()) {
            const QString callId = m_itemIdToCallId.value(requestId).value(itemId);
            if (!callId.isEmpty()) {
                message->handleToolCallComplete(callId);
            } else {
                LOG_MESSAGE(
                    QString("ERROR: OpenAIResponses - No call_id mapping found for item_id: %1")
                        .arg(itemId));
            }
        } else if (!item.isEmpty() && item["type"].toString() == "function_call") {
            const QString callId = item["call_id"].toString();
            if (!callId.isEmpty()) {
                message->handleToolCallComplete(callId);
            } else {
                LOG_MESSAGE(
                    QString("ERROR: OpenAIResponses - Function call done but call_id is empty"));
            }
        }
    } else if (eventType == "response.created") {
    } else if (eventType == "response.in_progress") {
    } else if (eventType == "response.completed") {
        const QJsonObject responseObj = data["response"].toObject();
        const QString statusStr = responseObj["status"].toString();

        if (m_dataBuffers[requestId].responseContent.isEmpty()) {
            const QString aggregatedText = extractAggregatedText(responseObj);
            if (!aggregatedText.isEmpty()) {
                m_dataBuffers[requestId].responseContent = aggregatedText;
            }
        }

        message->handleStatus(statusStr);
        handleMessageComplete(requestId);
    } else if (eventType == "response.incomplete") {
        const QJsonObject responseObj = data["response"].toObject();

        if (!responseObj.isEmpty()) {
            const QString statusStr = responseObj["status"].toString();

            if (m_dataBuffers[requestId].responseContent.isEmpty()) {
                const QString aggregatedText = extractAggregatedText(responseObj);
                if (!aggregatedText.isEmpty()) {
                    m_dataBuffers[requestId].responseContent = aggregatedText;
                }
            }

            message->handleStatus(statusStr);
        } else {
            message->handleStatus("incomplete");
        }

        handleMessageComplete(requestId);
    } else if (!eventType.isEmpty()) {
        LOG_MESSAGE(
            QString("WARNING: OpenAIResponses - Unhandled event type '%1' for request %2\nData: %3")
                .arg(eventType)
                .arg(requestId)
                .arg(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))));
    }
}

void OpenAIResponsesClient::emitPendingThinkingBlocks(const QString &requestId)
{
    if (!m_messages.contains(requestId)) {
        return;
    }

    OpenAIResponsesMessage *message = m_messages[requestId];
    const auto thinkingBlocks = message->getCurrentThinkingContent();

    if (thinkingBlocks.isEmpty()) {
        return;
    }

    const int alreadyEmitted = m_emittedThinkingBlocksCount.value(requestId, 0);
    const int totalBlocks = thinkingBlocks.size();

    for (int i = alreadyEmitted; i < totalBlocks; ++i) {
        const auto *thinkingContent = thinkingBlocks[i];

        if (thinkingContent->thinking().trimmed().isEmpty()) {
            continue;
        }

        emit thinkingBlockReceived(
            requestId, thinkingContent->thinking(), thinkingContent->signature());
    }

    m_emittedThinkingBlocksCount[requestId] = totalBlocks;
}

void OpenAIResponsesClient::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId)) {
        return;
    }

    OpenAIResponsesMessage *message = m_messages[requestId];

    emitPendingThinkingBlocks(requestId);

    if (message->state() == MessageState::RequiresToolExecution) {
        const auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            return;
        }

        for (const auto *toolContent : toolUseContent) {
            const auto toolStringName
                = tools()->stringName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);
            tools()->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }
    }
}

void OpenAIResponsesClient::cleanupRequest(const RequestID &requestId)
{
    if (m_messages.contains(requestId)) {
        OpenAIResponsesMessage *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_itemIdToCallId.remove(requestId);
    m_emittedThinkingBlocksCount.remove(requestId);

    tools()->cleanupRequest(requestId);
}

QString OpenAIResponsesClient::extractAggregatedText(const QJsonObject &responseObj)
{
    if (responseObj.contains("output_text")) {
        const QString outputText = responseObj["output_text"].toString();
        if (!outputText.isEmpty()) {
            return outputText;
        }
    }

    QString aggregated;
    if (responseObj.contains("output")) {
        const QJsonArray output = responseObj["output"].toArray();
        for (const auto &item : output) {
            const QJsonObject itemObj = item.toObject();
            if (itemObj["type"].toString() == "message" && itemObj.contains("content")) {
                const QJsonArray content = itemObj["content"].toArray();
                for (const auto &contentItem : content) {
                    const QJsonObject contentObj = contentItem.toObject();
                    if (contentObj["type"].toString() == "output_text") {
                        aggregated += contentObj["text"].toString();
                    }
                }
            }
        }
    }
    return aggregated;
}

QString OpenAIResponsesClient::extractReasoningText(const QJsonObject &item)
{
    QString reasoningText;

    if (item.contains("summary")) {
        const QJsonArray summary = item["summary"].toArray();
        for (const auto &summaryItem : summary) {
            const QJsonObject summaryObj = summaryItem.toObject();
            if (summaryObj["type"].toString() == "summary_text") {
                reasoningText = summaryObj["text"].toString();
                break;
            }
        }
    }

    if (reasoningText.isEmpty() && item.contains("content")) {
        const QJsonArray content = item["content"].toArray();
        QStringList texts;
        for (const auto &contentItem : content) {
            const QJsonObject contentObj = contentItem.toObject();
            if (contentObj["type"].toString() == "reasoning_text") {
                texts.append(contentObj["text"].toString());
            }
        }
        if (!texts.isEmpty()) {
            reasoningText = texts.join("\n");
        }
    }

    return reasoningText;
}

} // namespace QodeAssist::LLMCore
