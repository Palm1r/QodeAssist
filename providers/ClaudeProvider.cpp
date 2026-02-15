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

#include "llmcore/ClaudeConfig.hpp"
#include "llmcore/ClaudeRequest.hpp"
#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
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
    return LLMCore::ClaudeRequest::messagesEndpoint();
}

QString ClaudeProvider::chatEndpoint() const
{
    return LLMCore::ClaudeRequest::messagesEndpoint();
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
    const QJsonObject &config,
    bool isToolsEnabled,
    bool isThinkingEnabled)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    LLMCore::ClaudeConfig::applyTo(request, config);

    auto result = LLMCore::ClaudeConfig::validate(request);
    if (result.adjusted) {
        LOG_MESSAGE(QString("Warning: %1").arg(result.warning));
    }

    if (isToolsEnabled) {
        auto filter = (type == LLMCore::RequestType::QuickRefactoring)
            ? LLMCore::RunToolsFilter::OnlyRead
            : LLMCore::RunToolsFilter::ALL;
        auto tools = m_toolsManager->getToolsDefinitions(LLMCore::ToolSchemaFormat::Claude, filter);
        if (!tools.isEmpty()) {
            request["tools"] = tools;
            LOG_MESSAGE(QString("Added %1 tools to Claude request").arg(tools.size()));
        }
    }
}

QList<QString> ClaudeProvider::getInstalledModels(const QString &baseUrl)
{
    LLMCore::ClaudeListModelsRequest listModelsRequest;
    QNetworkRequest request = listModelsRequest.createNetworkRequest(baseUrl, apiKey());

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QList<QString> models;
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
        models = LLMCore::ClaudeListModelsRequest::parseResponse(response);
    } else {
        LOG_MESSAGE(QString("Error fetching Claude models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> ClaudeProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{
    return LLMCore::ValidationUtils::validateRequestFields(
        request, LLMCore::ClaudeConfig::requestTemplate());
}

QString ClaudeProvider::apiKey() const
{
    return Settings::providerSettings().claudeApiKey();
}

void ClaudeProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    LLMCore::ClaudeRequest::prepareNetworkRequest(networkRequest, apiKey());
}

LLMCore::ProviderID ClaudeProvider::providerID() const
{
    return LLMCore::ClaudeRequest::providerID();
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

bool ClaudeProvider::supportThinking() const
{
    return true;
}

bool ClaudeProvider::supportImage() const
{
    return true;
}

void ClaudeProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("ClaudeProvider: Cancelling request %1").arg(requestId));
    LLMCore::Provider::cancelRequest(requestId);
    cleanupRequest(requestId);
}

LLMCore::IToolsManager *ClaudeProvider::toolsManager() const
{
    return m_toolsManager;
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
        LLMCore::ClaudeResponse *message = m_messages[requestId];
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
        LLMCore::ClaudeResponse *message = m_messages[requestId];
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

    LLMCore::ClaudeResponse *message = m_messages[requestId];
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
    LLMCore::ClaudeResponse *response = m_messages.value(requestId);
    if (!response && event["type"].toString() == "message_start") {
        response = new LLMCore::ClaudeResponse(this);
        m_messages[requestId] = response;
        LOG_MESSAGE(QString("Created ClaudeResponse for request %1").arg(requestId));
    }

    if (!response) {
        return;
    }

    LLMCore::StreamEvent streamEvent = response->processEvent(event);

    switch (streamEvent.type) {
    case LLMCore::StreamEvent::Type::MessageStart:
        emit continuationStarted(requestId);
        break;

    case LLMCore::StreamEvent::Type::TextDelta:
        m_dataBuffers[requestId].responseContent += streamEvent.text;
        emit partialResponseReceived(requestId, streamEvent.text);
        break;

    case LLMCore::StreamEvent::Type::ThinkingBlockComplete:
        emit thinkingBlockReceived(requestId, streamEvent.thinking, streamEvent.signature);
        break;

    case LLMCore::StreamEvent::Type::RedactedThinkingBlockComplete:
        emit redactedThinkingBlockReceived(requestId, streamEvent.signature);
        break;

    case LLMCore::StreamEvent::Type::MessageComplete:
        handleMessageComplete(requestId);
        break;

    default:
        break;
    }
}

void ClaudeProvider::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    LLMCore::ClaudeResponse *message = m_messages[requestId];

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
        LLMCore::ClaudeResponse *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}

} // namespace QodeAssist::Providers
