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

#include <LLMCore/ToolsManager.hpp>

#include <QJsonArray>
#include "tools/ToolsRegistration.hpp"
#include <QJsonDocument>
#include <QJsonObject>

#include "pluginllmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

OllamaProvider::OllamaProvider(QObject *parent)
    : PluginLLMCore::Provider(parent)
    , m_client(new ::LLMCore::OllamaClient(url(), apiKey(), QString(), this))
{
    Tools::registerQodeAssistTools(m_client->tools());

    connect(
        m_client->tools(),
        &::LLMCore::ToolsManager::toolExecutionComplete,
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
    PluginLLMCore::PromptTemplate *prompt,
    PluginLLMCore::ContextData context,
    PluginLLMCore::RequestType type,
    bool isToolsEnabled,
    bool isThinkingEnabled)
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

    auto applyThinkingMode = [&request]() {
        request["enable_thinking"] = true;
        QJsonObject options = request["options"].toObject();
        options["temperature"] = 1.0;
        request["options"] = options;
    };

    if (type == PluginLLMCore::RequestType::CodeCompletion) {
        applySettings(Settings::codeCompletionSettings());
    } else if (type == PluginLLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();
        applySettings(qrSettings);
        
        if (isThinkingEnabled) {
            applyThinkingMode();
            LOG_MESSAGE(QString("OllamaProvider: Thinking mode enabled for QuickRefactoring"));
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();
        applySettings(chatSettings);

        if (isThinkingEnabled) {
            applyThinkingMode();
            LOG_MESSAGE(QString("OllamaProvider: Thinking mode enabled for Chat"));
        }
    }

    if (isToolsEnabled) {
        PluginLLMCore::RunToolsFilter filter = PluginLLMCore::RunToolsFilter::ALL;
        if (type == PluginLLMCore::RequestType::QuickRefactoring) {
            filter = PluginLLMCore::RunToolsFilter::OnlyRead;
        }

        auto toolsDefinitions = m_client->tools()->getToolsDefinitions();
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(
                QString("OllamaProvider: Added %1 tools to request").arg(toolsDefinitions.size()));
        }
    }
}

QFuture<QList<QString>> OllamaProvider::getInstalledModels(const QString &url)
{
    QNetworkRequest request(QString("%1%2").arg(url, "/api/tags"));
    prepareNetworkRequest(request);

    return httpClient()->get(request).then([](const QByteArray &data) {
        QList<QString> models;
        QJsonObject jsonObject = QJsonDocument::fromJson(data).object();
        QJsonArray modelArray = jsonObject["models"].toArray();

        for (const QJsonValue &value : modelArray) {
            QJsonObject modelObject = value.toObject();
            models.append(modelObject["name"].toString());
        }
        return models;
    }).onFailed([](const std::exception &e) {
        LOG_MESSAGE(QString("Error fetching models: %1").arg(e.what()));
        return QList<QString>{};
    });
}

QList<QString> OllamaProvider::validateRequest(const QJsonObject &request, PluginLLMCore::TemplateType type)
{
    const auto fimReq = QJsonObject{
        {"keep_alive", {}},
        {"model", {}},
        {"stream", {}},
        {"prompt", {}},
        {"suffix", {}},
        {"system", {}},
        {"images", QJsonArray{}},
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
        {"messages", QJsonArray{{QJsonObject{{"role", {}}, {"content", {}}, {"images", QJsonArray{}}}}}},
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

    return PluginLLMCore::ValidationUtils::validateRequestFields(
        request, type == PluginLLMCore::TemplateType::FIM ? fimReq : messageReq);
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

PluginLLMCore::ProviderID OllamaProvider::providerID() const
{
    return PluginLLMCore::ProviderID::Ollama;
}

void OllamaProvider::sendRequest(
    const PluginLLMCore::RequestID &requestId, const QUrl &url, const QJsonObject &payload)
{
    m_dataBuffers[requestId].clear();

    m_requestUrls[requestId] = url;
    m_originalRequests[requestId] = payload;

    QNetworkRequest networkRequest(url);
    prepareNetworkRequest(networkRequest);

    LOG_MESSAGE(QString("OllamaProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    httpClient()->postStreaming(requestId, networkRequest, payload);
}

bool OllamaProvider::supportsTools() const
{
    return true;
}

bool OllamaProvider::supportImage() const
{
    return true;
}

bool OllamaProvider::supportThinking() const
{
    return true;
}

void OllamaProvider::cancelRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("OllamaProvider: Cancelling request %1").arg(requestId));
    PluginLLMCore::Provider::cancelRequest(requestId);
    cleanupRequest(requestId);
}

void OllamaProvider::onDataReceived(
    const QodeAssist::PluginLLMCore::RequestID &requestId, const QByteArray &data)
{
    PluginLLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
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
    const QodeAssist::PluginLLMCore::RequestID &requestId, std::optional<QString> error)
{
    if (error) {
        LOG_MESSAGE(QString("OllamaProvider request %1 failed: %2").arg(requestId, *error));
        emit requestFailed(requestId, *error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        OllamaMessage *message = m_messages[requestId];
        if (message->state() == PluginLLMCore::MessageState::RequiresToolExecution) {
            LOG_MESSAGE(QString("Waiting for tools to complete for %1").arg(requestId));
            return;
        }
    }

    QString finalText;
    if (m_messages.contains(requestId)) {
        OllamaMessage *message = m_messages[requestId];

        for (auto block : message->currentBlocks()) {
            if (auto textContent = qobject_cast<PluginLLMCore::TextContent *>(block)) {
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
                auto toolStringName = m_client->tools()->displayName(tool->name());
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
        && message->state() == PluginLLMCore::MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
        LOG_MESSAGE(QString("Cleared message state for continuation request %1").arg(requestId));
    }

    if (data.contains("thinking")) {
        QString thinkingDelta = data["thinking"].toString();
        if (!thinkingDelta.isEmpty()) {
            message->handleThinkingDelta(thinkingDelta);
            LOG_MESSAGE(QString("OllamaProvider: Received thinking delta, length=%1")
                            .arg(thinkingDelta.length()));
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
                    if (qobject_cast<PluginLLMCore::TextContent *>(block)) {
                        hasTextContent = true;
                        break;
                    }
                }

                if (hasTextContent) {
                    PluginLLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
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
                if (qobject_cast<PluginLLMCore::TextContent *>(block)) {
                    hasTextContent = true;
                    break;
                }
            }

            if (hasTextContent) {
                PluginLLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
                buffers.responseContent += content;
                emit partialResponseReceived(requestId, content);
            }
        }
    }

    if (data["done"].toBool()) {
        if (data.contains("signature")) {
            QString signature = data["signature"].toString();
            message->handleThinkingComplete(signature);
            LOG_MESSAGE(QString("OllamaProvider: Set thinking signature, length=%1")
                            .arg(signature.length()));
        }

        message->handleDone(true);
        handleMessageComplete(requestId);
    }
}

void OllamaProvider::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    OllamaMessage *message = m_messages[requestId];

    emitThinkingBlocks(requestId, message);

    if (message->state() == PluginLLMCore::MessageState::RequiresToolExecution) {
        LOG_MESSAGE(QString("Ollama message requires tool execution for %1").arg(requestId));

        auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            LOG_MESSAGE(
                QString("WARNING: No tools to execute for %1 despite RequiresToolExecution state")
                    .arg(requestId));
            return;
        }

        for (auto toolContent : toolUseContent) {
            auto toolStringName = m_client->tools()->displayName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);

            LOG_MESSAGE(
                QString("Executing tool: name=%1, id=%2, input=%3")
                    .arg(toolContent->name())
                    .arg(toolContent->id())
                    .arg(
                        QString::fromUtf8(
                            QJsonDocument(toolContent->input()).toJson(QJsonDocument::Compact))));

            m_client->tools()->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }

    } else {
        LOG_MESSAGE(QString("Ollama message marked as complete for %1").arg(requestId));
    }
}

void OllamaProvider::cleanupRequest(const PluginLLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("Cleaning up Ollama request %1").arg(requestId));

    if (m_messages.contains(requestId)) {
        auto msg = m_messages.take(requestId);
        msg->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_thinkingEmitted.remove(requestId);
    m_thinkingStarted.remove(requestId);
    m_client->tools()->cleanupRequest(requestId);
}

void OllamaProvider::emitThinkingBlocks(const QString &requestId, OllamaMessage *message)
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
        LOG_MESSAGE(QString("Emitted thinking block for request %1, thinking length=%2, signature "
                            "length=%3")
                        .arg(requestId)
                        .arg(thinkingContent->thinking().length())
                        .arg(thinkingContent->signature().length()));
    }
    m_thinkingEmitted.insert(requestId);
}

::LLMCore::ToolsManager *OllamaProvider::toolsManager() const
{
    return m_client->tools();
}

} // namespace QodeAssist::Providers
