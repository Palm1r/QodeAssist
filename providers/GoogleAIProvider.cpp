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

#include "GoogleAIProvider.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QtCore/qurlquery.h>

#include "llmcore/ValidationUtils.hpp"
#include "logger/Logger.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProviderSettings.hpp"

namespace QodeAssist::Providers {

GoogleAIProvider::GoogleAIProvider(QObject *parent)
    : LLMCore::Provider(parent)
    , m_toolsManager(new Tools::ToolsManager(this))
{
    connect(
        m_toolsManager,
        &Tools::ToolsManager::toolExecutionComplete,
        this,
        &GoogleAIProvider::onToolExecutionComplete);
}

QString GoogleAIProvider::name() const
{
    return "Google AI";
}

QString GoogleAIProvider::url() const
{
    return "https://generativelanguage.googleapis.com/v1beta";
}

QString GoogleAIProvider::completionEndpoint() const
{
    return {};
}

QString GoogleAIProvider::chatEndpoint() const
{
    return {};
}

bool GoogleAIProvider::supportsModelListing() const
{
    return true;
}

void GoogleAIProvider::prepareRequest(
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
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.maxTokens();
        generationConfig["temperature"] = settings.temperature();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        request["generationConfig"] = generationConfig;
    };

    auto applyThinkingMode = [&request](const auto &settings) {
        QJsonObject generationConfig;
        generationConfig["maxOutputTokens"] = settings.thinkingMaxTokens();

        if (settings.useTopP())
            generationConfig["topP"] = settings.topP();
        if (settings.useTopK())
            generationConfig["topK"] = settings.topK();

        generationConfig["temperature"] = 1.0;

        QJsonObject thinkingConfig;
        thinkingConfig["includeThoughts"] = true;
        int budgetTokens = settings.thinkingBudgetTokens();
        if (budgetTokens != -1) {
            thinkingConfig["thinkingBudget"] = budgetTokens;
        }

        generationConfig["thinkingConfig"] = thinkingConfig;
        request["generationConfig"] = generationConfig;
    };

    if (type == LLMCore::RequestType::CodeCompletion) {
        applyModelParams(Settings::codeCompletionSettings());
    } else if (type == LLMCore::RequestType::QuickRefactoring) {
        const auto &qrSettings = Settings::quickRefactorSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(qrSettings);
        } else {
            applyModelParams(qrSettings);
        }
    } else {
        const auto &chatSettings = Settings::chatAssistantSettings();

        if (isThinkingEnabled) {
            applyThinkingMode(chatSettings);
        } else {
            applyModelParams(chatSettings);
        }
    }

    if (isToolsEnabled) {
        LLMCore::RunToolsFilter filter = LLMCore::RunToolsFilter::ALL;
        if (type == LLMCore::RequestType::QuickRefactoring) {
            filter = LLMCore::RunToolsFilter::OnlyRead;
        }

        auto toolsDefinitions = m_toolsManager->getToolsDefinitions(
            LLMCore::ToolSchemaFormat::Google, filter);
        if (!toolsDefinitions.isEmpty()) {
            request["tools"] = toolsDefinitions;
            LOG_MESSAGE(QString("Added %1 tools to Google AI request").arg(toolsDefinitions.size()));
        }
    }
}

QList<QString> GoogleAIProvider::getInstalledModels(const QString &url)
{
    QList<QString> models;

    QNetworkAccessManager manager;
    QNetworkRequest request(QString("%1/models?key=%2").arg(url, apiKey()));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObject = jsonResponse.object();

        if (jsonObject.contains("models")) {
            QJsonArray modelArray = jsonObject["models"].toArray();
            models.clear();

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
    } else {
        LOG_MESSAGE(QString("Error fetching Google AI models: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
    return models;
}

QList<QString> GoogleAIProvider::validateRequest(
    const QJsonObject &request, LLMCore::TemplateType type)
{
    QJsonObject templateReq;

    templateReq = QJsonObject{
        {"contents", QJsonArray{}},
        {"system_instruction", QJsonArray{}},
        {"generationConfig",
         QJsonObject{
             {"temperature", {}},
             {"maxOutputTokens", {}},
             {"topP", {}},
             {"topK", {}},
             {"thinkingConfig",
              QJsonObject{{"thinkingBudget", {}}, {"includeThoughts", {}}}}}},
        {"safetySettings", QJsonArray{}},
        {"tools", QJsonArray{}}};

    return LLMCore::ValidationUtils::validateRequestFields(request, templateReq);
}

QString GoogleAIProvider::apiKey() const
{
    return Settings::providerSettings().googleAiApiKey();
}

void GoogleAIProvider::prepareNetworkRequest(QNetworkRequest &networkRequest) const
{
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl url = networkRequest.url();
    QUrlQuery query(url.query());
    query.addQueryItem("key", apiKey());
    url.setQuery(query);
    networkRequest.setUrl(url);
}

LLMCore::ProviderID GoogleAIProvider::providerID() const
{
    return LLMCore::ProviderID::GoogleAI;
}

void GoogleAIProvider::sendRequest(
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

    LOG_MESSAGE(
        QString("GoogleAIProvider: Sending request %1 to %2").arg(requestId, url.toString()));

    emit httpClient()->sendRequest(request);
}

bool GoogleAIProvider::supportsTools() const
{
    return true;
}

bool GoogleAIProvider::supportThinking() const
{
    return true;
}

void GoogleAIProvider::cancelRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("GoogleAIProvider: Cancelling request %1").arg(requestId));
    LLMCore::Provider::cancelRequest(requestId);
    cleanupRequest(requestId);
}

void GoogleAIProvider::onDataReceived(
    const QodeAssist::LLMCore::RequestID &requestId, const QByteArray &data)
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

            LOG_MESSAGE(fullError);
            emit requestFailed(requestId, fullError);
            cleanupRequest(requestId);
            return;
        }
    }

    LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
    QStringList lines = buffers.rawStreamBuffer.processData(data);

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonObject chunk = parseEventLine(line);
        if (chunk.isEmpty())
            continue;

        processStreamChunk(requestId, chunk);
    }
}

void GoogleAIProvider::onRequestFinished(
    const QodeAssist::LLMCore::RequestID &requestId, bool success, const QString &error)
{
    if (!success) {
        LOG_MESSAGE(QString("GoogleAIProvider request %1 failed: %2").arg(requestId, error));
        emit requestFailed(requestId, error);
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
        
        if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
            LOG_MESSAGE(QString("Waiting for tools to complete for %1").arg(requestId));
            m_dataBuffers.remove(requestId);
            return;
        }
    }

    if (m_dataBuffers.contains(requestId)) {
        const LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
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

void GoogleAIProvider::onToolExecutionComplete(
    const QString &requestId, const QHash<QString, QString> &toolResults)
{
    if (!m_messages.contains(requestId) || !m_requestUrls.contains(requestId)) {
        LOG_MESSAGE(QString("ERROR: Missing data for continuation request %1").arg(requestId));
        cleanupRequest(requestId);
        return;
    }

    for (auto it = toolResults.begin(); it != toolResults.end(); ++it) {
        GoogleMessage *message = m_messages[requestId];
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

    GoogleMessage *message = m_messages[requestId];
    QJsonObject continuationRequest = m_originalRequests[requestId];
    QJsonArray contents = continuationRequest["contents"].toArray();

    contents.append(message->toProviderFormat());

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["parts"] = message->createToolResultParts(toolResults);
    contents.append(userMessage);

    continuationRequest["contents"] = contents;

    sendRequest(requestId, m_requestUrls[requestId], continuationRequest);
}

void GoogleAIProvider::processStreamChunk(const QString &requestId, const QJsonObject &chunk)
{
    if (!chunk.contains("candidates")) {
        return;
    }

    GoogleMessage *message = m_messages.value(requestId);
    if (!message) {
        message = new GoogleMessage(this);
        m_messages[requestId] = message;
        LOG_MESSAGE(QString("Created NEW GoogleMessage for request %1").arg(requestId));

        if (m_dataBuffers.contains(requestId)) {
            emit continuationStarted(requestId);
            LOG_MESSAGE(QString("Starting continuation for request %1").arg(requestId));
        }
    } else if (
        m_dataBuffers.contains(requestId)
        && message->state() == LLMCore::MessageState::RequiresToolExecution) {
        message->startNewContinuation();
        m_emittedThinkingBlocksCount[requestId] = 0;
        LOG_MESSAGE(QString("Cleared message state for continuation request %1").arg(requestId));
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

                            LLMCore::DataBuffers &buffers = m_dataBuffers[requestId];
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
                LOG_MESSAGE(QString("Google AI error: %1").arg(errorMessage));
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
            LOG_MESSAGE(QString("Google AI tokens: %1 (thoughts: %2, output: %3)")
                            .arg(totalTokenCount)
                            .arg(thoughtsTokenCount)
                            .arg(candidatesTokenCount));
        }
    }
}

void GoogleAIProvider::emitPendingThinkingBlocks(const QString &requestId)
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
        emit thinkingBlockReceived(
            requestId, 
            thinkingContent->thinking(), 
            thinkingContent->signature());
    }

    m_emittedThinkingBlocksCount[requestId] = totalBlocks;
}

void GoogleAIProvider::handleMessageComplete(const QString &requestId)
{
    if (!m_messages.contains(requestId))
        return;

    GoogleMessage *message = m_messages[requestId];

    if (message->state() == LLMCore::MessageState::RequiresToolExecution) {
        LOG_MESSAGE(QString("Google AI message requires tool execution for %1").arg(requestId));

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
        LOG_MESSAGE(QString("Google AI message marked as complete for %1").arg(requestId));
    }
}

void GoogleAIProvider::cleanupRequest(const LLMCore::RequestID &requestId)
{
    LOG_MESSAGE(QString("Cleaning up Google AI request %1").arg(requestId));

    if (m_messages.contains(requestId)) {
        GoogleMessage *message = m_messages.take(requestId);
        message->deleteLater();
    }

    m_dataBuffers.remove(requestId);
    m_requestUrls.remove(requestId);
    m_originalRequests.remove(requestId);
    m_emittedThinkingBlocksCount.remove(requestId);
    m_failedRequests.remove(requestId);
    m_toolsManager->cleanupRequest(requestId);
}

} // namespace QodeAssist::Providers
