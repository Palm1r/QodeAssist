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

#include "LLMClientInterface.hpp"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "CodeHandler.hpp"
#include "context/DocumentContextReader.hpp"
#include "context/Utils.hpp"
#include "llmcore/PromptTemplateManager.hpp"
#include "llmcore/ProvidersManager.hpp"
#include "logger/Logger.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include <llmcore/RequestConfig.hpp>

namespace QodeAssist {

LLMClientInterface::LLMClientInterface(
    const Settings::GeneralSettings &generalSettings,
    const Settings::CodeCompletionSettings &completeSettings,
    LLMCore::IProviderRegistry &providerRegistry,
    LLMCore::IPromptProvider *promptProvider,
    LLMCore::RequestHandlerBase &requestHandler,
    Context::IDocumentReader &documentReader,
    IRequestPerformanceLogger &performanceLogger)
    : m_generalSettings(generalSettings)
    , m_completeSettings(completeSettings)
    , m_providerRegistry(providerRegistry)
    , m_promptProvider(promptProvider)
    , m_requestHandler(requestHandler)
    , m_documentReader(documentReader)
    , m_performanceLogger(performanceLogger)
    , m_contextManager(new Context::ContextManager(this))
{
    connect(
        &m_requestHandler,
        &LLMCore::RequestHandler::completionReceived,
        this,
        &LLMClientInterface::sendCompletionToClient);
}

Utils::FilePath LLMClientInterface::serverDeviceTemplate() const
{
    return "QodeAssist";
}

void LLMClientInterface::startImpl()
{
    emit started();
}

void LLMClientInterface::sendData(const QByteArray &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;

    QJsonObject request = doc.object();
    QString method = request["method"].toString();

    if (method == "initialize") {
        handleInitialize(request);
    } else if (method == "initialized") {
        // TODO make initilizied handler
    } else if (method == "shutdown") {
        handleShutdown(request);
    } else if (method == "textDocument/didOpen") {
        handleTextDocumentDidOpen(request);
    } else if (method == "getCompletionsCycling") {
        QString requestId = request["id"].toString();
        m_performanceLogger.startTimeMeasurement(requestId);
        handleCompletion(request);
    } else if (method == "$/cancelRequest") {
        handleCancelRequest(request);
    } else if (method == "exit") {
        // TODO make exit handler
    } else {
        LOG_MESSAGE(QString("Unknown method: %1").arg(method));
    }
}

void LLMClientInterface::handleCancelRequest(const QJsonObject &request)
{
    QString id = request["params"].toObject()["id"].toString();
    if (m_requestHandler.cancelRequest(id)) {
        LOG_MESSAGE(QString("Request %1 cancelled successfully").arg(id));
    } else {
        LOG_MESSAGE(QString("Request %1 not found").arg(id));
    }
}

void LLMClientInterface::handleInitialize(const QJsonObject &request)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = request["id"];

    QJsonObject result;
    QJsonObject capabilities;
    capabilities["textDocumentSync"] = 1;
    capabilities["completionProvider"] = QJsonObject{{"resolveProvider", false}};
    capabilities["hoverProvider"] = true;
    result["capabilities"] = capabilities;

    QJsonObject serverInfo;
    serverInfo["name"] = "QodeAssist LSP Server";
    serverInfo["version"] = "0.1";
    result["serverInfo"] = serverInfo;

    response["result"] = result;

    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
}

void LLMClientInterface::handleShutdown(const QJsonObject &request)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = request["id"];
    response["result"] = QJsonValue();

    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
}

void LLMClientInterface::handleTextDocumentDidOpen(const QJsonObject &request) {}

void LLMClientInterface::handleInitialized(const QJsonObject &request)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["method"] = "initialized";
    response["params"] = QJsonObject();

    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
}

void LLMClientInterface::handleExit(const QJsonObject &request)
{
    emit finished();
}

void LLMClientInterface::handleCompletion(const QJsonObject &request)
{
    auto filePath = Context::extractFilePathFromRequest(request);
    auto documentInfo = m_documentReader.readDocument(filePath);
    if (!documentInfo.document) {
        LOG_MESSAGE("Error: Document is not available for" + filePath);
        return;
    }

    auto updatedContext = prepareContext(request, documentInfo);

    bool isPreset1Active = m_contextManager->isSpecifyCompletion(documentInfo);

    const auto providerName = !isPreset1Active ? m_generalSettings.ccProvider()
                                               : m_generalSettings.ccPreset1Provider();
    const auto modelName = !isPreset1Active ? m_generalSettings.ccModel()
                                            : m_generalSettings.ccPreset1Model();
    const auto url = !isPreset1Active ? m_generalSettings.ccUrl()
                                      : m_generalSettings.ccPreset1Url();

    const auto provider = m_providerRegistry.getProviderByName(providerName);

    if (!provider) {
        LOG_MESSAGE(QString("No provider found with name: %1").arg(providerName));
        return;
    }

    auto templateName = !isPreset1Active ? m_generalSettings.ccTemplate()
                                         : m_generalSettings.ccPreset1Template();

    auto promptTemplate = m_promptProvider->getTemplateByName(templateName);

    if (!promptTemplate) {
        LOG_MESSAGE(QString("No template found with name: %1").arg(templateName));
        return;
    }

    // TODO refactor to dynamic presets system
    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::CodeCompletion;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    // TODO refactor networking
    if (provider->providerID() == LLMCore::ProviderID::GoogleAI) {
        QString stream = m_completeSettings.stream() ? QString{"streamGenerateContent?alt=sse"}
                                                     : QString{"generateContent?"};
        config.url = QUrl(QString("%1/models/%2:%3").arg(url, modelName, stream));
    } else {
        config.url = QUrl(QString("%1%2").arg(
            url,
            promptTemplate->type() == LLMCore::TemplateType::FIM ? provider->completionEndpoint()
                                                                 : provider->chatEndpoint()));
        config.providerRequest = {{"model", modelName}, {"stream", m_completeSettings.stream()}};
    }
    config.apiKey = provider->apiKey();
    config.multiLineCompletion = m_completeSettings.multiLineCompletion();

    const auto stopWords = QJsonArray::fromStringList(config.promptTemplate->stopWords());
    if (!stopWords.isEmpty())
        config.providerRequest["stop"] = stopWords;

    QString systemPrompt;
    if (m_completeSettings.useSystemPrompt())
        systemPrompt.append(
            m_completeSettings.useUserMessageTemplateForCC()
                    && promptTemplate->type() == LLMCore::TemplateType::Chat
                ? m_completeSettings.systemPromptForNonFimModels()
                : m_completeSettings.systemPrompt());
    if (updatedContext.fileContext.has_value())
        systemPrompt.append(updatedContext.fileContext.value());

    if (m_completeSettings.useOpenFilesContext()) {
        if (provider->providerID() == LLMCore::ProviderID::LlamaCpp) {
            for (const auto openedFilePath : m_contextManager->openedFiles({filePath})) {
                if (!updatedContext.filesMetadata) {
                    updatedContext.filesMetadata = QList<LLMCore::FileMetadata>();
                }
                updatedContext.filesMetadata->append({openedFilePath.first, openedFilePath.second});
            }
        } else {
            systemPrompt.append(m_contextManager->openedFilesContext({filePath}));
        }
    }

    updatedContext.systemPrompt = systemPrompt;

    if (promptTemplate->type() == LLMCore::TemplateType::Chat) {
        QString userMessage;
        if (m_completeSettings.useUserMessageTemplateForCC()) {
            userMessage = m_completeSettings.processMessageToFIM(
                updatedContext.prefix.value_or(""), updatedContext.suffix.value_or(""));
        } else {
            userMessage = updatedContext.prefix.value_or("") + updatedContext.suffix.value_or("");
        }

        // TODO refactor add message
        QVector<LLMCore::Message> messages;
        messages.append({"user", userMessage});
        updatedContext.history = messages;
    }

    config.provider->prepareRequest(
        config.providerRequest,
        promptTemplate,
        updatedContext,
        LLMCore::RequestType::CodeCompletion);

    auto errors = config.provider->validateRequest(config.providerRequest, promptTemplate->type());
    if (!errors.isEmpty()) {
        LOG_MESSAGE("Validate errors for fim request:");
        LOG_MESSAGES(errors);
        return;
    }
    m_requestHandler.sendLLMRequest(config, request);
}

LLMCore::ContextData LLMClientInterface::prepareContext(
    const QJsonObject &request, const Context::DocumentInfo &documentInfo)
{
    QJsonObject params = request["params"].toObject();
    QJsonObject doc = params["doc"].toObject();
    QJsonObject position = doc["position"].toObject();
    int cursorPosition = position["character"].toInt();
    int lineNumber = position["line"].toInt();

    Context::DocumentContextReader
        reader(documentInfo.document, documentInfo.mimeType, documentInfo.filePath);
    return reader.prepareContext(lineNumber, cursorPosition, m_completeSettings);
}

Context::ContextManager *LLMClientInterface::contextManager() const
{
    return m_contextManager;
}

void LLMClientInterface::sendCompletionToClient(
    const QString &completion, const QJsonObject &request, bool isComplete)
{
    auto filePath = Context::extractFilePathFromRequest(request);
    auto documentInfo = m_documentReader.readDocument(filePath);
    bool isPreset1Active = m_contextManager->isSpecifyCompletion(documentInfo);

    auto templateName = !isPreset1Active ? m_generalSettings.ccTemplate()
                                         : m_generalSettings.ccPreset1Template();

    auto promptTemplate = m_promptProvider->getTemplateByName(templateName);

    QJsonObject position = request["params"].toObject()["doc"].toObject()["position"].toObject();

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response[LanguageServerProtocol::idKey] = request["id"];

    QJsonObject result;
    QJsonArray completions;
    QJsonObject completionItem;

    LOG_MESSAGE(QString("Completions before filter: \n%1").arg(completion));

    QString processedCompletion
        = promptTemplate->type() == LLMCore::TemplateType::Chat
                  && m_completeSettings.smartProcessInstuctText()
              ? CodeHandler::processText(completion, Context::extractFilePathFromRequest(request))
              : completion;

    completionItem[LanguageServerProtocol::textKey] = processedCompletion;
    QJsonObject range;
    range["start"] = position;
    QJsonObject end = position;
    end["character"] = position["character"].toInt() + processedCompletion.length();
    range["end"] = end;
    completionItem[LanguageServerProtocol::rangeKey] = range;
    completionItem[LanguageServerProtocol::positionKey] = position;
    completions.append(completionItem);
    result["completions"] = completions;
    result[LanguageServerProtocol::isIncompleteKey] = !isComplete;
    response[LanguageServerProtocol::resultKey] = result;

    LOG_MESSAGE(
        QString("Completions: \n%1")
            .arg(QString::fromUtf8(QJsonDocument(completions).toJson(QJsonDocument::Indented))));

    LOG_MESSAGE(
        QString("Full response: \n%1")
            .arg(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Indented))));

    QString requestId = request["id"].toString();
    m_performanceLogger.endTimeMeasurement(requestId);
    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
}

} // namespace QodeAssist
