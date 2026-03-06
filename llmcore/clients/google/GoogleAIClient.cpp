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

#include "GoogleAIClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include "llmcore/core/Log.hpp"

#include "GoogleMessage.hpp"

#include <llmcore/network/SSEUtils.hpp>

namespace QodeAssist::LLMCore {

GoogleAIClient::GoogleAIClient(QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &GoogleAIClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &GoogleAIClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &GoogleAIClient::handleToolExecutionComplete);
}

GoogleAIClient::GoogleAIClient(ApiKey apiKey, QObject *parent)
    : BaseClient(parent)
    , m_httpClient(new HttpClient(this))
    , m_apiKey(std::move(apiKey))
{
    connect(m_httpClient, &HttpClient::dataReceived, this, &GoogleAIClient::handleDataReceived);
    connect(
        m_httpClient, &HttpClient::requestFinished, this, &GoogleAIClient::handleRequestFinished);
    connect(
        tools(),
        &ToolsManager::toolExecutionComplete,
        this,
        &GoogleAIClient::handleToolExecutionComplete);
}

void GoogleAIClient::setApiKey(const QString &apiKey)
{
    m_apiKey = [apiKey]() { return apiKey; };
}

QString GoogleAIClient::apiKey() const
{
    return m_apiKey ? m_apiKey() : QString{};
}

QNetworkRequest GoogleAIClient::prepareNetworkRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl requestUrl = request.url();
    QUrlQuery query(requestUrl.query());
    query.addQueryItem("key", apiKey());
    requestUrl.setQuery(query);
    request.setUrl(requestUrl);

    return request;
}

QFuture<QList<QString>> GoogleAIClient::listModels(const QString &baseUrl)
{
    QUrl url(QString("%1/models?key=%2").arg(baseUrl, apiKey()));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    return m_httpClient->get(request)
        .then([](const QByteArray &data) {
            QList<QString> models;
            QJsonObject jsonObject = QJsonDocument::fromJson(data).object();

            if (jsonObject.contains("models")) {
                QJsonArray modelArray = jsonObject["models"].toArray();
                for (const QJsonValue &value : modelArray) {
                    QJsonObject modelObject = value.toObject();
                    if (modelObject.contains("name")) {
                        QString modelName = modelObject["name"].toString();
                        if (modelName.contains("/")) {
                            modelName = modelName.split("/").last();
                        }
                        models.append(modelName);
                    }
                }
            }
            return models;
        })
        .onFailed([](const std::exception &e) {
            qCDebug(llmGoogleLog).noquote() << QString("Error fetching models: %1").arg(e.what());
            return QList<QString>{};
        });
}

void GoogleAIClient::sendMessage(
    const RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    if (!m_messages.contains(requestId)) {
        m_dataBuffers[requestId].clear();
    }

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    qCDebug(llmGoogleLog).noquote() << QString("Sending request %1 to %2").arg(requestId, url.toString());

    QNetworkRequest request = prepareNetworkRequest(url);
    m_httpClient->postStreaming(requestId, request, payload);
}

void GoogleAIClient::cancelRequest(const RequestID &requestId)
{
    qCDebug(llmGoogleLog).noquote() << QString("Cancelling request %1").arg(requestId);
    m_httpClient->cancelRequest(requestId);
    cleanupRequest(requestId);
}

void GoogleAIClient::handleDataReceived(const QString &requestId, const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("error")) {
            QJsonObject error = obj["error"].toObject();
            QString errorMessage = error["message"].toString();
            int errorCode = error["code"].toInt();
            QString fullError
                = QString("Google AI API Error %1: %2").arg(errorCode).arg(errorMessage);

            qCDebug(llmGoogleLog).noquote() << fullError;
            emit requestFailed(requestId, fullError);
            cleanupRequest(requestId);
            return;
        }
    }

    DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonObject chunk = SSEUtils::parseEventLine(line);
        if (chunk.isEmpty())
            continue;

        processStreamChunk(requestId, chunk);
    }
}

void GoogleAIClient::handleRequestFinished(const QString &requestId, std::optional<QString> error)
{
    if (error) {
        qCDebug(llmGoogleLog).noquote() << QString("Request %1 failed: %2").arg(requestId, *error);
        emit requestFailed(requestId, *error);
        cleanupRequest(requestId);
        return;
    }

    if (m_failedRequests.contains(requestId)) {
        cleanupRequest(requestId);
        return;
    }

    emitPendingThinkingBlocks(requestId);

    if (m_messages.contains(requestId)) {
        GoogleMessage *message = m_messages[requestId];

        handleMessageComplete(requestId);

        if (message->state() == MessageState::RequiresToolExecution) {
            qCDebug(llmGoogleLog).noquote() << QString("Waiting for tools to complete for %1").arg(requestId);
            m_dataBuffers.remove(requestId);
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const DataBuffers &buffers = m_dataBuffers[requestId];
        if (!buffers.responseContent.isEmpty()) {
            emit fullResponseReceived(requestId, buffers.responseContent);
        } else {
            emit fullResponseReceived(requestId, QString());
        }
    } else {
        emit fullResponseReceived(requestId, QString());
    }

    cleanupRequest(requestId);
}

void GoogleAIClient::handleToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        qCDebug(llmGoogleLog).noquote() << QString("ERROR: Missing data for continuation request %1").arg(requestId);
        cleanupRequest(requestId);
        return;
    }

    for (auto it = toolResults.begin(); it != toolResults.end(); ++it) {
        GoogleMessage *message = m_messages[requestId];
        auto toolContent = message->getCurrentToolUseContent();
        for (auto tool : toolContent) {
            if (tool->id() == it.key()) {
                auto toolStringName = tools()->stringName(tool->name());
                emit toolExecutionCompleted(
                    requestId, tool->id(), toolStringName, toolResults[tool->id()]);
                break;
            }
        }
    }

    GoogleMessage *message = m_messages[requestId];
    QJsonObject continuationRequest = m_originalRequests[requestId];
    QJsonArray contents = continuationRequest["contents"].toArray();

    contents.append(message->toProviderFormat());

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["parts"] = message->createToolResultParts(toolResults);
    contents.append(userMessage);

    continuationRequest["contents"] = contents;

    sendMessage(requestId, m_requestUrls[requestId], continuationRequest);
}

void GoogleAIClient::processStreamChunk(const QString &requestId, const QJsonObject &chunk)
{
    if (!chunk.contains("candidates")) {
        return;
    }

    GoogleMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new GoogleMessage(this);
        m_messages[requestId] = message;
        qCDebug(llmGoogleLog).noquote() << QString("Created NEW GoogleMessage for request %1").arg(requestId);

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
            qCDebug(llmGoogleLog).noquote() << QString("Starting continuation for request %1").arg(requestId);
        }
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        m_emittedThinkingBlocksCount[requestId] = 0;
        qCDebug(llmGoogleLog).noquote() << QString("Cleared message state for continuation request %1").arg(requestId);
    }

    QJsonArray candidates = chunk["candidates"].toArray();
    for (const QJsonValue &candidate : candidates) {
        QJsonObject candidateObj = candidate.toObject();

        if (candidateObj.contains("content")) {
            QJsonObject content = candidateObj["content"].toObject();
            if (content.contains("parts")) {
                QJsonArray parts = content["parts"].toArray();
                for (const QJsonValue &part : parts) {
                    QJsonObject partObj = part.toObject();

                    if (partObj.contains("text")) {
                        QString text = partObj["text"].toString();
                        bool isThought = partObj.value("thought").toBool(false);

                        if (isThought) {
                            message->handleThoughtDelta(text);

                            if (partObj.contains("signature")) {
                                QString signature = partObj["signature"].toString();
                                message->handleThoughtSignature(signature);
                            }
                        } else {
                            emitPendingThinkingBlocks(requestId);

                            message->handleContentDelta(text);

                            DataBuffers &buffers = m_dataBuffers[requestId];
                            buffers.responseContent += text;
                            emit partialResponseReceived(requestId, text);
                        }
                    }

                    if (partObj.contains("thoughtSignature")) {
                        QString signature = partObj["thoughtSignature"].toString();
                        message->handleThoughtSignature(signature);
                    }

                    if (partObj.contains("functionCall")) {
                        emitPendingThinkingBlocks(requestId);

                        QJsonObject functionCall = partObj["functionCall"].toObject();
                        QString name = functionCall["name"].toString();
                        QJsonObject args = functionCall["args"].toObject();

                        message->handleFunctionCallStart(name);
                        message->handleFunctionCallArgsDelta(
                            QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)));
                        message->handleFunctionCallComplete();
                    }
                }
            }
        }

        if (candidateObj.contains("finishReason")) {
            QString finishReason = candidateObj["finishReason"].toString();
            message->handleFinishReason(finishReason);

            if (message->isErrorFinishReason()) {
                QString errorMessage = message->getErrorMessage();
                qCDebug(llmGoogleLog).noquote() << QString("Google AI error: %1").arg(errorMessage);
                m_failedRequests.insert(requestId);
                emit requestFailed(requestId, errorMessage);
                return;
            }
        }
    }

    if (chunk.contains("usageMetadata")) {
        QJsonObject usageMetadata = chunk["usageMetadata"].toObject();
        int thoughtsTokenCount = usageMetadata.value("thoughtsTokenCount").toInt(0);
        int candidatesTokenCount = usageMetadata.value("candidatesTokenCount").toInt(0);
        int totalTokenCount = usageMetadata.value("totalTokenCount").toInt(0);

        if (totalTokenCount > 0) {
            qCDebug(llmGoogleLog).noquote() << QString("Google AI tokens: %1 (thoughts: %2, output: %3)")
                            .arg(totalTokenCount)
                            .arg(thoughtsTokenCount)
                            .arg(candidatesTokenCount);
        }
    }
}

void GoogleAIClient::emitPendingThinkingBlocks(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    GoogleMessage *message = m_messages[requestId];
    auto thinkingBlocks = message->getCurrentThinkingContent();

    if (thinkingBlocks.isEmpty())
        return;

    int alreadyEmitted = m_emittedThinkingBlocksCount.value(requestId, 0);
    int totalBlocks = thinkingBlocks.size();

    for (int i = alreadyEmitted; i < totalBlocks; ++i) {
        auto thinkingContent = thinkingBlocks[i];

        if (thinkingContent->thinking().trimmed().isEmpty()) {
            continue;
        }

        emit thinkingBlockReceived(
            requestId,
            thinkingContent->thinking(),
            thinkingContent->signature());
    }

    m_emittedThinkingBlocksCount[requestId] = totalBlocks;
}

void GoogleAIClient::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    GoogleMessage *message = m_messages[requestId];

    if (message->state() == MessageState::RequiresToolExecution) {
        qCDebug(llmGoogleLog).noquote() << QString("Message requires tool execution for %1").arg(requestId);

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            qCDebug(llmGoogleLog).noquote() << QString("No tools to execute for %1").arg(requestId);
            return;
        }

        for (auto toolContent : toolUseContent) {
            auto toolStringName = tools()->stringName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);
            tools()->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }

    } else {
        qCDebug(llmGoogleLog).noquote() << QString("Message marked as complete for %1").arg(requestId);
    }
}

void GoogleAIClient::cleanupRequest(const RequestID &requestId)
{
    qCDebug(llmGoogleLog).noquote() << QString("Cleaning up request %1").arg(requestId);

    if (m_messages.contains(requestId)) {
        GoogleMessage *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_emittedThinkingBlocksCount.remove(requestId);
    m_failedRequests.remove(requestId);

    tools()->cleanupRequest(requestId);
}

} // namespace QodeAssist::LLMCore
