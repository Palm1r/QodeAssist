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

#include "OllamaProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QtCore/qeventloop.h>

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

OllamaProvider::OllamaProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsManager(new Tools::ToolsManager(this))
{
    connect(
        m_toolsManager,
        &Tools::ToolsManager::toolExecutionComplete,
        this,
        &OllamaProvider::onToolExecutionComplete);
}

QString OllamaProvider::name() const
{
    return "Ollama";
}

QString OllamaProvider::url() const
{
    return "http://localhost:11434";
}

QString OllamaProvider::completionEndpoint() const
{
    return "/api/generate";
}

QString OllamaProvider::chatEndpoint() const
{
    return "/api/chat";
}

bool OllamaProvider::supportsModelListing() const
{
    return true;
}

void OllamaProvider::prepareRequest(
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

    auto applySettings = [&request](const auto &settings) {
        QJsonObject options;
        options["num_predict"] = settings.maxTokens();
        options["temperature"] = settings.temperature();
        options["stop"] = request.take("stop");

        if (settings.useTopP())
            options["top_p"] = settings.topP();
        if (settings.useTopK())
            options["top_k"] = settings.topK();
        if (settings.useFrequencyPenalty())
            options["frequency_penalty"] = settings.frequencyPenalty();
        if (settings.usePresencePenalty())
            options["presence_penalty"] = settings.presencePenalty();

        request["options"] = options;
        request["keep_alive"] = settings.ollamaLivetime();
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applySettings(Settings::codeCompletionSettings());
    } else {
        applySettings(Settings::chatAssistantSettings());
    }

    if (isToolsEnabled) {
        auto toolsDefinitions = m_toolsManager->toolsFactory()->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::Ollama);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(
                QString("OllamaProvider: Added %1 tools to request").arg(toolsDefinitions.size()));
        }
    }
}

QList<QString> OllamaProvider::getInstalledModels(const QString &url)
{
    QList<QString> models;
    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1%2").arg(url, "/api/tags"));
    prepareNetworkRequest(request);
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();
        QJsonArray modelArray = jsonObject["models"].toArray();

        for (const QJsonValue &value : modelArray) {
            QJsonObject modelObject = value.toObject();
            QString modelName = modelObject["name"].toString();
            models.append(modelName);
        }
    } else {
        LOG_MESSAGE(QString("Error fetching models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> OllamaProvider::validateRequest(const QJsonObject &request, LLMCore::TemplateType type)
{
    const auto fimReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"prompt", {}},
        {"suffix", {}},
        {"system", {}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    const auto messageReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}}}}},
        {"tools", QJsonArray{}},
        {"options",
         QJsonObject{
             {"temperature", {}},
             {"stop", {}},
             {"top_p", {}},
             {"top_k", {}},
             {"num_predict", {}},
             {"frequency_penalty", {}},
             {"presence_penalty", {}}}}};

    return LLMCore::ValidationUtils::validateRequestFields(
        request, type == LLMCore::TemplateType::FIM ? fimReq : messageReq);
}

QString OllamaProvider::apiKey() const
{
    return {};
}

void OllamaProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const auto key = Settings::providerSettings().ollamaBasicAuthApiKey();
    if (!key.isEmpty()) {
        networkRequest.setRawHeader("Authorization", "Basic " + key.toLatin1());
    }
}

LLMCore::ProviderID OllamaProvider::providerID() const
{
    return LLMCore::ProviderID::Ollama;
}

void OllamaProvider::sendRequest(
    const LLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_dataBuffers[requestId].clear();

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LLMCore::HttpRequest
        request{.networkRequest = networkRequest, .requestId = requestId, .payload = payload};

    LOG_MESSAGE(QString("OllamaProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

bool OllamaProvider::supportsTools() const
{
    return true;
}

void OllamaProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("OllamaProvider: Cancelling request %1").arg(requestId));
    LLMCore::Provider::cancelRequest(requestId);
    cleanupRequest(requestId);
}

void OllamaProvider::onDataReceived(
    const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
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
            LOG_MESSAGE(QString("Failed to parse JSON: %1").arg(error.errorString()));
            continue;
        }

        QJsonObject obj = doc.object();

        if (obj.contains("error") && !obj["error"].toString().isEmpty()) {
            LOG_MESSAGE("Error in Ollama response: " + obj["error"].toString());
            continue;
        }

        processStreamData(requestId, obj);
    }
}

void OllamaProvider::onRequestFinished(
    const QodeAssist::LLMCore::RequestID &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("OllamaProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        OllamaMessage *message = m_messages[requestId];
        if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
            LOG_MESSAGE(QString("Waiting for tools to complete for %1").arg(requestId));
            return;
        }
    }

    QString finalText;
    if (m_messages.contains(requestId)) {
        OllamaMessage *message = m_messages[requestId];

        for (auto block : message->currentBlocks()) {
            if (auto textContent = qobject_cast<LLMCore::TextContent *>(block)) {
                finalText += textContent->text();
            }
        }

        if (!finalText.isEmpty()) {
            LOG_MESSAGE(QString("Emitting full response for %1, length=%2")
                            .arg(requestId)
                            .arg(finalText.length()));
            emit fullResponseReceived(requestId, finalText);
        }
    }

    cleanupRequest(requestId);
}

void OllamaProvider::onToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId)) {
        LOG_MESSAGE(QString("ERROR: No message found for request %1").arg(requestId));
        cleanupRequest(requestId);
        return;
    }

    if (!m_requestUrls.contains(requestId) || !m_originalRequests.contains(requestId)) {
        LOG_MESSAGE(QString("ERROR: Missing data for continuation request %1").arg(requestId));
        cleanupRequest(requestId);
        return;
    }

    LOG_MESSAGE(QString("Tool execution complete for Ollama request %1").arg(requestId));

    OllamaMessage *message = m_messages[requestId];

    for (auto it = toolResults.begin(); it != toolResults.end(); ++it) {
        auto toolContent = message->getCurrentToolUseContent();
        for (auto tool : toolContent) {
            if (tool->id() == it.key()) {
                auto toolStringName = m_toolsManager->toolsFactory()->getStringName(tool->name());
                emit toolExecutionCompleted(requestId, tool->id(), toolStringName, it.value());
                break;
            }
        }
    }

    QJsonObject continuationRequest = m_originalRequests[requestId];
    QJsonArray messages = continuationRequest["messages"].toArray();

    QJsonObject assistantMessage = message->toProviderFormat();
    messages.append(assistantMessage);

    LOG_MESSAGE(QString("Assistant message with tool_calls:\n%1")
                    .arg(
                        QString::fromUtf8(
                            QJsonDocument(assistantMessage).toJson(QJsonDocument::Indented))));

    QJsonArray toolResultMessages = message->createToolResultMessages(toolResults);
    for (const auto &toolMsg : toolResultMessages) {
        messages.append(toolMsg);
        LOG_MESSAGE(QString("Tool result message:\n%1")
                        .arg(
                            QString::fromUtf8(
                                QJsonDocument(toolMsg.toObject()).toJson(QJsonDocument::Indented))));
    }

    continuationRequest["messages"] = messages;

    LOG_MESSAGE(QString("Sending continuation request for %1 with %2 tool results")
                    .arg(requestId)
                    .arg(toolResults.size()));

    sendRequest(requestId, m_requestUrls[requestId], continuationRequest);
}

void OllamaProvider::processStreamData(const QString &requestId, const QJsonObject &data)
{
    OllamaMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new OllamaMessage(this);
        m_messages[requestId] = message;
        LOG_MESSAGE(QString("Created NEW OllamaMessage for request %1").arg(requestId));

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
            LOG_MESSAGE(QString("Starting continuation for request %1").arg(requestId));
        }
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == LLMCore::MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
        LOG_MESSAGE(QString("Cleared message state for continuation request %1").arg(requestId));
    }

    if (data.contains("message")) {
        QJsonObject messageObj = data["message"].toObject();

        if (messageObj.contains("content")) {
            QString content = messageObj["content"].toString();
            if (!content.isEmpty()) {
                message->handleContentDelta(content);

                bool hasTextContent = false;
                for (auto block : message->currentBlocks()) {
                    if (qobject_cast<LLMCore::TextContent *>(block)) {
                        hasTextContent = true;
                        break;
                    }
                }

                if (hasTextContent) {
                    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
                    buffers.responseContent += content;
                    emit partialResponseReceived(requestId, content);
                }
            }
        }

        if (messageObj.contains("tool_calls")) {
            QJsonArray toolCalls = messageObj["tool_calls"].toArray();
            LOG_MESSAGE(
                QString("OllamaProvider: Found %1 structured tool calls").arg(toolCalls.size()));
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
                if (qobject_cast<LLMCore::TextContent *>(block)) {
                    hasTextContent = true;
                    break;
                }
            }

            if (hasTextContent) {
                LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
                buffers.responseContent += content;
                emit partialResponseReceived(requestId, content);
            }
        }
    }

    if (data["done"].toBool()) {
        message->handleDone(true);
        handleMessageComplete(requestId);
    }
}

void OllamaProvider::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    OllamaMessage *message = m_messages[requestId];

    if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
        LOG_MESSAGE(QString("Ollama message requires tool execution for %1").arg(requestId));

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            LOG_MESSAGE(
                QString("WARNING: No tools to execute for %1 despite RequiresToolExecution state")
                    .arg(requestId));
            return;
        }

        for (auto toolContent : toolUseContent) {
            auto toolStringName = m_toolsManager->toolsFactory()->getStringName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);

            LOG_MESSAGE(
                QString("Executing tool: name=%1, id=%2, input=%3")
                    .arg(toolContent->name())
                    .arg(toolContent->id())
                    .arg(
                        QString::fromUtf8(
                            QJsonDocument(toolContent->input()).toJson(QJsonDocument::Compact))));

            m_toolsManager->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }

    } else {
        LOG_MESSAGE(QString("Ollama message marked as complete for %1").arg(requestId));
    }
}

void OllamaProvider::cleanupRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("Cleaning up Ollama request %1").arg(requestId));

    if (m_messages.contains(requestId)) {
        auto msg = m_messages.take(requestId);
        msg->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}
} // namespace QodeAssist::Providers
