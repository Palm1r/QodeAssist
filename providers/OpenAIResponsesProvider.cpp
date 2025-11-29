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

#include "OpenAIResponsesProvider.hpp"
#include "OpenAIResponses/ResponseObject.hpp"

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace QodeAssist::Providers {

OpenAIResponsesProvider::OpenAIResponsesProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsManager(new Tools::ToolsManager(this))
{
    connect(
        m_toolsManager,
        &Tools::ToolsManager::toolExecutionComplete,
        this,
        &OpenAIResponsesProvider::onToolExecutionComplete);
}

QString OpenAIResponsesProvider::name() const
{
    return "OpenAI Responses";
}

QString OpenAIResponsesProvider::url() const
{
    return "https://api.openai.com";
}

QString OpenAIResponsesProvider::completionEndpoint() const
{
    return "/v1/responses";
}

QString OpenAIResponsesProvider::chatEndpoint() const
{
    return "/v1/responses";
}

bool OpenAIResponsesProvider::supportsModelListing() const
{
    return true;
}

void OpenAIResponsesProvider::prepareRequest(
    QJsonObject &request,
    LLMCore::PromptTemplate *prompt,
    LLMCore::ContextData context,
    const LLMCore::InputParameters &params)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    const auto *responsesParams = dynamic_cast<const LLMCore::OpenAIResponsesInputParameters*>(&params);
    
    QJsonObject reasoning;
    if (params.enableThinking) {
        if (responsesParams && responsesParams->thinkingEffort) {
            reasoning["effort"] = *responsesParams->thinkingEffort;
        } else {
            reasoning["effort"] = "medium";
        }

        if (params.maxTokens) {
            request["max_output_tokens"] = *params.maxTokens;
        }
        request["temperature"] = 1.0;
        
        if (responsesParams && responsesParams->store) {
            request["store"] = *responsesParams->store;
        }

        if (responsesParams && responsesParams->includeReasoningContent) {
            QJsonArray include;
            include.append("reasoning.encrypted_content");
            request["include"] = include;
        }
    } else {
        reasoning["effort"] = "none";

        if (params.maxTokens) {
            request["max_output_tokens"] = *params.maxTokens;
        }
        if (params.temperature) {
            request["temperature"] = *params.temperature;
        }
        if (params.topP) {
            request["top_p"] = *params.topP;
        }
    }

    request["reasoning"] = reasoning;
    request["stream"] = params.stream;

    if (params.enableTools) {
        const auto toolsDefinitions = m_toolsManager->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::OpenAI, LLMCore::RunToolsFilter::ALL);
        if (!toolsDefinitions.isEmpty()) {
            QJsonArray responsesTools;

            for (const QJsonValue &toolValue : toolsDefinitions) {
                const QJsonObject tool = toolValue.toObject();
                if (tool.contains("function")) {
                    const QJsonObject functionObj = tool["function"].toObject();
                    QJsonObject responsesTool;
                    responsesTool["type"] = "function";
                    responsesTool["name"] = functionObj["name"];
                    responsesTool["description"] = functionObj["description"];
                    responsesTool["parameters"] = functionObj["parameters"];
                    responsesTools.append(responsesTool);
                }
            }
            request["tools"] = responsesTools;
        }
    }
}

QList<QString> OpenAIResponsesProvider::getInstalledModels(const QString &url)
{
    static const QStringList excludedModelPatterns = {
        "dall-e", "whisper", "tts", "davinci", "babbage", "omni", "embedding"};

    QList<QString> models;
    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1/v1/models").arg(url));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!apiKey().isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray responseData = reply->readAll();
        const QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        const QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains("data")) {
            const QJsonArray modelArray = jsonObject["data"].toArray();
            models.reserve(modelArray.size());

            for (const QJsonValue &value : modelArray) {
                const QJsonObject modelObject = value.toObject();
                if (modelObject.contains("id")) {
                    const QString modelId = modelObject["id"].toString();

                    bool shouldExclude = false;
                    for (const QString &pattern : excludedModelPatterns) {
                        if (modelId.contains(pattern)) {
                            shouldExclude = true;
                            break;
                        }
                    }

                    if (!shouldExclude) {
                        models.append(modelId);
                    }
                }
            }
        }
    } else {
        LOG_MESSAGE(QString("Error fetching OpenAI models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> OpenAIResponsesProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    Q_UNUSED(type);

    QList<QString> errors;

    if (!request.contains("input")) {
        errors.append("Missing required field: input");
        return errors;
    }

    const QJsonValue inputValue = request["input"];
    if (!inputValue.isString() && !inputValue.isArray()) {
        errors.append("Field 'input' must be either a string or an array");
    }

    if (request.contains("max_output_tokens") && !request["max_output_tokens"].isDouble()) {
        errors.append("Field 'max_output_tokens' must be a number");
    }

    if (request.contains("temperature") && !request["temperature"].isDouble()) {
        errors.append("Field 'temperature' must be a number");
    }

    if (request.contains("top_p") && !request["top_p"].isDouble()) {
        errors.append("Field 'top_p' must be a number");
    }

    if (request.contains("reasoning") && !request["reasoning"].isObject()) {
        errors.append("Field 'reasoning' must be an object");
    }

    if (request.contains("stream") && !request["stream"].isBool()) {
        errors.append("Field 'stream' must be a boolean");
    }

    if (request.contains("tools") && !request["tools"].isArray()) {
        errors.append("Field 'tools' must be an array");
    }

    return errors;
}

QString OpenAIResponsesProvider::apiKey() const
{
    return Settings::providerSettings().openAiApiKey();
}

void OpenAIResponsesProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (!apiKey().isEmpty()) {
        networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey()).toUtf8());
    }
}

LLMCore::ProviderID OpenAIResponsesProvider::providerID() const
{
    return LLMCore::ProviderID::OpenAIResponses;
}

void OpenAIResponsesProvider::sendRequest(
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

    emit httpClient()->sendRequest(request);
}

bool OpenAIResponsesProvider::supportsTools() const
{
    return true;
}

bool OpenAIResponsesProvider::supportImage() const
{
    return true;
}

bool OpenAIResponsesProvider::supportThinking() const
{
    return true;
}

void OpenAIResponsesProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    LLMCore::Provider::cancelRequest(requestId);
    cleanupRequest(requestId);
}

void OpenAIResponsesProvider::onDataReceived(
    const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data)
{
    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    const QStringList lines = buffers.rawStreamBuffer.processData(data);

    QString currentEventType;

    for (const QString &line : lines) {
        const QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }

        if (line == "data: [DONE]") {
            LOG_MESSAGE(QString("OpenAIResponsesProvider: Received [DONE] for %1").arg(requestId));
            continue;
        }

        if (line.startsWith("event: ")) {
            currentEventType = line.mid(7).trimmed();
            LOG_MESSAGE(QString("OpenAIResponses: Event type: %1").arg(currentEventType));
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

void OpenAIResponsesProvider::onRequestFinished(
    const QodeAssist::LLMCore::RequestID &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("OpenAIResponses request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
        cleanupRequest(requestId);
        return;
    }

    if (m_messages.contains(requestId)) {
        OpenAIResponsesMessage *message = m_messages[requestId];
        if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
        if (!buffers.responseContent.isEmpty()) {
            emit fullResponseReceived(requestId, buffers.responseContent);
        }
    }

    cleanupRequest(requestId);
}

void OpenAIResponsesProvider::processStreamEvent(
    const QString &requestId, const QString &eventType, const QJsonObject &data)
{
    OpenAIResponsesMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new OpenAIResponsesMessage(this);
        m_messages[requestId] = message;

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
        }
    } else if (m_dataBuffers.contains(requestId)
               && message->state() == LLMCore::MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
    }

    if (eventType == "response.output_text.delta") {
        const QString delta = data["delta"].toString();
        if (!delta.isEmpty()) {
            m_dataBuffers[requestId].responseContent += delta;
            emit partialResponseReceived(requestId, delta);
        }
    } else if (eventType == "response.output_text.done") {
        const QString fullText = data["text"].toString();
        if (!fullText.isEmpty()) {
            m_dataBuffers[requestId].responseContent = fullText;
        }
    } else if (eventType == "response.output_item.added") {
        using namespace QodeAssist::OpenAIResponses;
        const QJsonObject item = data["item"].toObject();
        OutputItem outputItem = OutputItem::fromJson(item);

        LOG_MESSAGE(QString("OpenAIResponses: output_item.added type=%1")
                        .arg(item["type"].toString()));

        if (const auto *functionCall = outputItem.asFunctionCall()) {
            if (!functionCall->callId.isEmpty() && !functionCall->name.isEmpty()) {
                if (!m_itemIdToCallId.contains(requestId)) {
                    m_itemIdToCallId[requestId] = QHash<QString, QString>();
                }
                m_itemIdToCallId[requestId][functionCall->id] = functionCall->callId;
                message->handleToolCallStart(functionCall->callId, functionCall->name);
            }
        } else if (const auto *reasoning = outputItem.asReasoning()) {
            LOG_MESSAGE(QString("OpenAIResponses: Reasoning item added, id=%1").arg(reasoning->id));
            if (!reasoning->id.isEmpty()) {
                message->handleReasoningStart(reasoning->id);
            }
        }
    } else if (eventType == "response.reasoning_content.delta") {
        const QString itemId = data["item_id"].toString();
        const QString delta = data["delta"].toString();
        LOG_MESSAGE(QString("OpenAIResponses: Reasoning delta for %1, length=%2")
                        .arg(itemId)
                        .arg(delta.length()));
        if (!itemId.isEmpty() && !delta.isEmpty()) {
            message->handleReasoningDelta(itemId, delta);
        }
    } else if (eventType == "response.reasoning_content.done") {
        const QString itemId = data["item_id"].toString();
        LOG_MESSAGE(QString("OpenAIResponses: Reasoning done for %1").arg(itemId));
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
    } else if (eventType == "response.function_call_arguments.done"
               || eventType == "response.output_item.done") {
        const QString itemId = data["item_id"].toString();
        const QJsonObject item = data["item"].toObject();
        
        if (!item.isEmpty() && item["type"].toString() == "reasoning") {
            using namespace QodeAssist::OpenAIResponses;
            
            const QString finalItemId = itemId.isEmpty() ? item["id"].toString() : itemId;

            LOG_MESSAGE(QString("OpenAIResponses: Reasoning item structure:\n%1")
                            .arg(QString::fromUtf8(
                                QJsonDocument(item).toJson(QJsonDocument::Indented))));

            ReasoningOutput reasoningOutput = ReasoningOutput::fromJson(item);
            QString reasoningText;

            if (!reasoningOutput.summaryText.isEmpty()) {
                reasoningText = reasoningOutput.summaryText;
                LOG_MESSAGE(QString("OpenAIResponses: Using reasoning summary"));
            } else if (!reasoningOutput.contentTexts.isEmpty()) {
                reasoningText = reasoningOutput.contentTexts.join("\n");
                LOG_MESSAGE(QString("OpenAIResponses: Using reasoning content texts, parts=%1")
                                .arg(reasoningOutput.contentTexts.size()));
            }

            if (reasoningText.isEmpty()) {
                reasoningText = QString(
                    "[Reasoning process completed, but detailed thinking is not available in "
                    "streaming mode. The model has processed your request with extended reasoning "
                    "(effort: medium).]");
                LOG_MESSAGE(
                    QString("OpenAIResponses: Using placeholder for empty reasoning %1")
                        .arg(finalItemId));
            }

            if (!finalItemId.isEmpty()) {
                LOG_MESSAGE(QString("OpenAIResponses: Reasoning complete for %1, length=%2")
                                .arg(finalItemId)
                                .arg(reasoningText.length()));
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
    } else if (eventType == "response.completed") {
        using namespace QodeAssist::OpenAIResponses;
        const QJsonObject responseObj = data["response"].toObject();
        Response response = Response::fromJson(responseObj);

        const QString statusStr = responseObj["status"].toString();

        if (m_dataBuffers[requestId].responseContent.isEmpty()) {
            const QString aggregatedText = response.getAggregatedText();
            if (!aggregatedText.isEmpty()) {
                m_dataBuffers[requestId].responseContent = aggregatedText;
            }
        }

        message->handleStatus(statusStr);
        handleMessageComplete(requestId);
    }
}

void OpenAIResponsesProvider::emitPendingThinkingBlocks(const QString &requestId)
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

void OpenAIResponsesProvider::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId)) {
        return;
    }

    OpenAIResponsesMessage *message = m_messages[requestId];

    emitPendingThinkingBlocks(requestId);

    if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
        const auto toolUseContent = message->getCurrentToolUseContent();

        if (toolUseContent.isEmpty()) {
            return;
        }

        for (const auto *toolContent : toolUseContent) {
            const auto toolStringName
                = m_toolsManager->toolsFactory()->getStringName(toolContent->name());
            emit toolExecutionStarted(requestId, toolContent->id(), toolStringName);
            m_toolsManager->executeToolCall(
                requestId, toolContent->id(), toolContent->name(), toolContent->input());
        }
    }
}

void OpenAIResponsesProvider::onToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        LOG_MESSAGE(QString("ERROR: OpenAIResponses - Missing data for continuation request %1")
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
                    = m_toolsManager->toolsFactory()->getStringName(tool->name());
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

    sendRequest(requestId, m_requestUrls[requestId], continuationRequest);
}

void OpenAIResponsesProvider::cleanupRequest(const LLMCore::RequestID &requestId)
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
    m_toolsManager->cleanupRequest(requestId);
}

} // namespace QodeAssist::Providers

