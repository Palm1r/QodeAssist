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
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"

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
    LLMCore::RequestType type,
    bool isToolsEnabled,
    bool isThinkingEnabled)
{
    if (!prompt->isSupportProvider(providerID())) {
        LOG_MESSAGE(QString("Template %1 doesn't support %2 provider").arg(name(), prompt->name()));
    }

    prompt->prepareRequest(request, context);

    auto applyModelParams = [&request](const auto &settings) {
        request["max_output_tokens"] = settings.maxTokens();

        if (settings.useTopP()) {
            request["top_p"] = settings.topP();
        }
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QString effortStr = settings.openAIResponsesReasoningEffort.stringValue().toLower();
        if (effortStr.isEmpty()) {
            effortStr = "medium";
        }
        
        QJsonObject reasoning;
        reasoning["effort"] = effortStr;
        request["reasoning"] = reasoning;
        request["max_output_tokens"] = settings.thinkingMaxTokens();
        request["store"] = true;

        QJsonArray include;
        include.append("reasoning.encrypted_content");
        request["include"] = include;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();
        applyModelParams(qrSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();
        applyModelParams(chatSettings);

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        }
    }

    if (isToolsEnabled) {
        const LLMCore::RunToolsFilter filter = (type == LLMCore::RequestType::QuickRefactoring)
                                                   ? LLMCore::RunToolsFilter::OnlyRead
                                                   : LLMCore::RunToolsFilter::ALL;

        const auto toolsDefinitions
            = m_toolsManager->getToolsDefinitions(LLMCore::ToolSchemaFormat::OpenAI, filter);
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

    request["stream"] = true;
}

QList<QString> OpenAIResponsesProvider::getInstalledModels(const QString &url)
{
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
        } else {
            LOG_MESSAGE(QString("WARNING: OpenAIResponses - Response content is empty for %1, "
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
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == LLMCore::MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        emit continuationStarted(requestId);
    }

    if (eventType == "response.content_part.added") {
    } else if (eventType == "response.output_text.delta") {
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
    } else if (eventType == "response.content_part.done") {
    } else if (eventType == "response.output_item.added") {
        using namespace QodeAssist::OpenAIResponses;
        const QJsonObject item = data["item"].toObject();
        OutputItem outputItem = OutputItem::fromJson(item);

        if (const auto *functionCall = outputItem.asFunctionCall()) {
            if (!functionCall->callId.isEmpty() && !functionCall->name.isEmpty()) {
                if (!m_itemIdToCallId.contains(requestId)) {
                    m_itemIdToCallId[requestId] = QHash<QString, QString>();
                }
                m_itemIdToCallId[requestId][functionCall->id] = functionCall->callId;
                message->handleToolCallStart(functionCall->callId, functionCall->name);
            }
        } else if (const auto *reasoning = outputItem.asReasoning()) {
            if (!reasoning->id.isEmpty()) {
                message->handleReasoningStart(reasoning->id);
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
                LOG_MESSAGE(QString("ERROR: No call_id mapping found for item_id: %1").arg(itemId));
            }
        }
    } else if (
        eventType == "response.function_call_arguments.done"
        || eventType == "response.output_item.done") {
        const QString itemId = data["item_id"].toString();
        const QJsonObject item = data["item"].toObject();

        if (!item.isEmpty() && item["type"].toString() == "reasoning") {
            using namespace QodeAssist::OpenAIResponses;

            const QString finalItemId = itemId.isEmpty() ? item["id"].toString() : itemId;

            ReasoningOutput reasoningOutput = ReasoningOutput::fromJson(item);
            QString reasoningText;

            if (!reasoningOutput.summaryText.isEmpty()) {
                reasoningText = reasoningOutput.summaryText;
            } else if (!reasoningOutput.contentTexts.isEmpty()) {
                reasoningText = reasoningOutput.contentTexts.join("\n");
            }

            if (reasoningText.isEmpty()) {
                reasoningText = QString(
                    "[Reasoning process completed, but detailed thinking is not available in "
                    "streaming mode. The model has processed your request with extended reasoning.]");
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
    } else if (eventType == "response.incomplete") {
        using namespace QodeAssist::OpenAIResponses;
        const QJsonObject responseObj = data["response"].toObject();

        if (!responseObj.isEmpty()) {
            Response response = Response::fromJson(responseObj);
            const QString statusStr = responseObj["status"].toString();

            if (m_dataBuffers[requestId].responseContent.isEmpty()) {
                const QString aggregatedText = response.getAggregatedText();
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
        LOG_MESSAGE(QString("WARNING: OpenAIResponses - Unhandled event type '%1' for request %2\nData: %3")
                        .arg(eventType)
                        .arg(requestId)
                        .arg(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))));
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
            const auto toolStringName = m_toolsManager->toolsFactory()->getStringName(
                toolContent->name());
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
                const auto toolStringName = m_toolsManager->toolsFactory()->getStringName(
                    tool->name());
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
