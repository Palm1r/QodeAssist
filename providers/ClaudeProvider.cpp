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
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

ClaudeProvider::ClaudeProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsManager(new Tools::ToolsManager(this))
{
    connect(
        m_toolsManager,
        &Tools::ToolsManager::toolExecutionComplete,
        this,
        &ClaudeProvider::onToolExecutionComplete);
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

void ClaudeProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    LLMCore::RequestType type,
    bool isToolsEnabled)
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
        const auto &chatSettings = Settings::chatAssistantSettings();
        applyModelParams(chatSettings);

        if (chatSettings.enableThinkingMode()) {
            QJsonObject thinkingObj;
            thinkingObj["type"] = "enabled";
            thinkingObj["budget_tokens"] = chatSettings.thinkingBudgetTokens();
            request["thinking"] = thinkingObj;
            request["max_tokens"] = chatSettings.thinkingMaxTokens();
        }
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_toolsManager->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::Claude);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Claude request").arg(toolsDefinitions.size()));
        }
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
        {"stream", {}},
        {"tools", {}},
        {"thinking", QJsonObject{{"type", {}}, {"budget_tokens", {}}}}};

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
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    if (!m_messages.contains(requestId)) {
        m_dataBuffers[requestId].clear();
    }

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(QString("ClaudeProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

bool ClaudeProvider::supportsTools() const
{
    return true;
}

bool ClaudeProvider::supportThinking() const {
    return true;
};

void ClaudeProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("ClaudeProvider: Cancelling request %1").arg(requestId));
    LLMCore::Provider::cancelRequest(requestId);
    cleanupRequest(requestId);
}

void ClaudeProvider::onDataReceived(
    const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        QJsonObject responseObj = parseEventLine(line);
        if (responseObj.isEmpty())
            continue;

        processStreamEvent(requestId, responseObj);
    }
}

void ClaudeProvider::onRequestFinished(
    const QodeAssist::LLMCore::RequestID &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("ClaudeProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        ClaudeMessage *message = m_messages[requestId];
        if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
            LOG_MESSAGE(QString("Waiting for tools to complete for %1").arg(requestId));
            m_dataBuffers.remove(requestId);
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
        if (!buffers.responseContent.isEmpty()) {
            LOG_MESSAGE(QString("Emitting full response for %1").arg(requestId));
            emit fullResponseReceived(requestId, buffers.responseContent);
        }
    }

    cleanupRequest(requestId);
}

void ClaudeProvider::onToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        LOG_MESSAGE(QString("ERROR: Missing data for continuation request %1").arg(requestId));
        cleanupRequest(requestId);
        return;
    }

    LOG_MESSAGE(QString("Tool execution complete for Claude request %1").arg(requestId));

    for (auto it = toolResults.begin(); it != toolResults.end(); ++it) {
        ClaudeMessage *message = m_messages[requestId];
        auto toolContent = message->getCurrentToolUseContent();
        for (auto tool : toolContent) {
            if (tool->id() == it.key()) {
                auto toolStringName = m_toolsManager->toolsFactory()->getStringName(tool->name());
                emit toolExecutionCompleted(
                    requestId, tool->id(), toolStringName, toolResults[tool->id()]);
                break;
            }
        }
    }

    ClaudeMessage *message = m_messages[requestId];
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
        LOG_MESSAGE(QString("Thinking mode preserved for continuation: type=%1, budget=%2 tokens")
                        .arg(thinkingObj["type"].toString())
                        .arg(thinkingObj["budget_tokens"].toInt()));
    }
    
    LOG_MESSAGE(QString("Sending continuation request for %1 with %2 tool results")
                    .arg(requestId)
                    .arg(toolResults.size()));

    sendRequest(requestId, m_requestUrls[requestId], continuationRequest);
}

void ClaudeProvider::processStreamEvent(const QString &requestId, const QJsonObject &event)
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
            LOG_MESSAGE(QString("Created NEW ClaudeMessage for request %1").arg(requestId));
        } else {
            return;
        }
    }

    if (eventType == "message_start") {
        message->startNewContinuation();
        emit continuationStarted(requestId);
        LOG_MESSAGE(QString("Starting NEW continuation for request %1").arg(requestId));

    } else if (eventType == "content_block_start") {
        int index = event["index"].toInt();
        QJsonObject contentBlock = event["content_block"].toObject();
        QString blockType = contentBlock["type"].toString();

        LOG_MESSAGE(
            QString("Adding new content block: type=%1, index=%2").arg(blockType).arg(index));
        
        if (blockType == "thinking" || blockType == "redacted_thinking") {
            QJsonDocument eventDoc(event);
            LOG_MESSAGE(QString("content_block_start event for %1: %2")
                            .arg(blockType)
                            .arg(QString::fromUtf8(eventDoc.toJson(QJsonDocument::Compact))));
        }

        message->handleContentBlockStart(index, blockType, contentBlock);

    } else if (eventType == "content_block_delta") {
        int index = event["index"].toInt();
        QJsonObject delta = event["delta"].toObject();
        QString deltaType = delta["type"].toString();

        message->handleContentBlockDelta(index, deltaType, delta);

        if (deltaType == "text_delta") {
            QString text = delta["text"].toString();
            LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
            buffers.responseContent += text;
            emit partialResponseReceived(requestId, text);
        } else if (deltaType == "signature_delta") {
            QString signature = delta["signature"].toString();
        }

    } else if (eventType == "content_block_stop") {
        int index = event["index"].toInt();
        
        auto allBlocks = message->getCurrentBlocks();
        if (index < allBlocks.size()) {
            QString blockType = allBlocks[index]->type();
            if (blockType == "thinking" || blockType == "redacted_thinking") {
                QJsonDocument eventDoc(event);
                LOG_MESSAGE(QString("content_block_stop event for %1 at index %2: %3")
                                .arg(blockType)
                                .arg(index)
                                .arg(QString::fromUtf8(eventDoc.toJson(QJsonDocument::Compact))));
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
                        if (auto thinkingContent = qobject_cast<LLMCore::ThinkingContent *>(allBlocks[index])) {
                            thinkingContent->setSignature(signature);
                            LOG_MESSAGE(
                                QString("Updated thinking block signature from content_block_stop, "
                                        "signature length=%1")
                                    .arg(signature.length()));
                        }
                    }
                }
            } else if (blockType == "redacted_thinking") {
                QString signature = contentBlock["signature"].toString();
                if (!signature.isEmpty()) {
                    auto allBlocks = message->getCurrentBlocks();
                    if (index < allBlocks.size()) {
                        if (auto redactedContent = qobject_cast<LLMCore::RedactedThinkingContent *>(allBlocks[index])) {
                            redactedContent->setSignature(signature);
                            LOG_MESSAGE(
                                QString("Updated redacted_thinking block signature from content_block_stop, "
                                        "signature length=%1")
                                    .arg(signature.length()));
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
                LOG_MESSAGE(
                    QString("Emitted thinking block for request %1, thinking length=%2, signature length=%3")
                        .arg(requestId)
                        .arg(thinkingContent->thinking().length())
                        .arg(thinkingContent->signature().length()));
                break;
            }
        }

        auto redactedBlocks = message->getCurrentRedactedThinkingContent();
        for (auto redactedContent : redactedBlocks) {
            auto allBlocks = message->getCurrentBlocks();
            if (index < allBlocks.size() && allBlocks[index] == redactedContent) {
                emit redactedThinkingBlockReceived(requestId, redactedContent->signature());
                LOG_MESSAGE(
                    QString("Emitted redacted thinking block for request %1, signature length=%2")
                        .arg(requestId)
                        .arg(redactedContent->signature().length()));
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

void ClaudeProvider::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    ClaudeMessage *message = m_messages[requestId];

    if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
        LOG_MESSAGE(QString("Claude message requires tool execution for %1").arg(requestId));

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            LOG_MESSAGE(QString("No tools to execute for %1").arg(requestId));
            return;
        }

        for (auto toolContent : toolUseContent) {
            auto toolStringName = m_toolsManager->toolsFactory()->getStringName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);
            m_toolsManager->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }

    } else {
        LOG_MESSAGE(QString("Claude message marked as complete for %1").arg(requestId));
    }
}

void ClaudeProvider::cleanupRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("Cleaning up Claude request %1").arg(requestId));

    if (m_messages.contains(requestId)) {
        ClaudeMessage *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}

} // namespace QodeAssist::Providers
