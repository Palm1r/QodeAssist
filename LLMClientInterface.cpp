// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LLMClientInterface.hpp"

#include <LLMQore/BaseClient.hpp>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "CodeHandler.hpp"
#include "context/DocumentContextReader.hpp"
#include "context/Utils.hpp"
#include "logger/Logger.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include <pluginllmcore/RulesLoader.hpp>

namespace QodeAssist {

LLMClientInterface::LLMClientInterface(
    const Settings::GeneralSettings &generalSettings,
    const Settings::CodeCompletionSettings &completeSettings,
    PluginLLMCore::IProviderRegistry &providerRegistry,
    PluginLLMCore::IPromptProvider *promptProvider,
    Context::IDocumentReader &documentReader,
    IRequestPerformanceLogger &performanceLogger)
    : m_generalSettings(generalSettings)
    , m_completeSettings(completeSettings)
    , m_providerRegistry(providerRegistry)
    , m_promptProvider(promptProvider)
    , m_documentReader(documentReader)
    , m_performanceLogger(performanceLogger)
    , m_contextManager(new Context::ContextManager(this))
{
}

LLMClientInterface::~LLMClientInterface()
{
    handleCancelRequest();
}

Utils::FilePath LLMClientInterface::serverDeviceTemplate() const
{
    return "QodeAssist";
}

void LLMClientInterface::startImpl()
{
    emit started();
}

void LLMClientInterface::handleFullResponse(const QString &requestId, const QString &fullText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const RequestContext &ctx = it.value();
    sendCompletionToClient(fullText, ctx.originalRequest, true);

    m_activeRequests.erase(it);
    m_performanceLogger.endTimeMeasurement(requestId);
}

void LLMClientInterface::handleRequestFailed(const QString &requestId, const QString &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const RequestContext &ctx = it.value();
    QString enriched = ctx.provider ? ctx.provider->enrichErrorMessage(error) : error;

    LOG_MESSAGE(QString("Request %1 failed: %2").arg(requestId, enriched));

    // Send LSP error response to client
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response[LanguageServerProtocol::idKey] = ctx.originalRequest["id"];

    QJsonObject errorObject;
    errorObject["code"] = -32603; // Internal error code
    errorObject["message"] = enriched;
    response["error"] = errorObject;
    
    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
    
    m_activeRequests.erase(it);
    m_performanceLogger.endTimeMeasurement(requestId);
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
        handleCompletion(request);
    } else if (method == "$/cancelRequest") {
        handleCancelRequest();
    } else if (method == "exit") {
        // TODO make exit handler
    } else {
        LOG_MESSAGE(QString("Unknown method: %1").arg(method));
    }
}

void LLMClientInterface::handleCancelRequest()
{
    QSet<PluginLLMCore::Provider *> providers;
    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        if (it.value().provider) {
            providers.insert(it.value().provider);
        }
    }

    for (auto *provider : providers) {
        disconnect(provider->client(), nullptr, this, nullptr);
    }

    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        const RequestContext &ctx = it.value();
        if (ctx.provider) {
            ctx.provider->cancelRequest(it.key());
        }
    }

    m_activeRequests.clear();

    LOG_MESSAGE("All requests cancelled and state cleared");
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

void LLMClientInterface::sendErrorResponse(const QJsonObject &request, const QString &errorMessage)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response[LanguageServerProtocol::idKey] = request["id"];
    
    QJsonObject errorObject;
    errorObject["code"] = -32603; // Internal error code
    errorObject["message"] = errorMessage;
    response["error"] = errorObject;
    
    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
    
    // End performance measurement if it was started
    QString requestId = request["id"].toString();
    m_performanceLogger.endTimeMeasurement(requestId);
}

void LLMClientInterface::handleCompletion(const QJsonObject &request)
{
    auto filePath = Context::extractFilePathFromRequest(request);
    auto documentInfo = m_documentReader.readDocument(filePath);
    if (!documentInfo.document) {
        QString error = QString("Document is not available: %1").arg(filePath);
        LOG_MESSAGE("Error: " + error);
        sendErrorResponse(request, error);
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
        QString error = QString("No provider found with name: %1").arg(providerName);
        LOG_MESSAGE(error);
        sendErrorResponse(request, error);
        return;
    }

    auto templateName = !isPreset1Active ? m_generalSettings.ccTemplate()
                                         : m_generalSettings.ccPreset1Template();

    auto promptTemplate = m_promptProvider->getTemplateByName(templateName);

    if (!promptTemplate) {
        QString error = QString("No template found with name: %1").arg(templateName);
        LOG_MESSAGE(error);
        sendErrorResponse(request, error);
        return;
    }

    QJsonObject payload{{"model", modelName}, {"stream", true}};

    const auto stopWords = QJsonArray::fromStringList(promptTemplate->stopWords());
    if (!stopWords.isEmpty())
        payload["stop"] = stopWords;

    QString systemPrompt;
    if (m_completeSettings.useSystemPrompt())
        systemPrompt.append(
            m_completeSettings.useUserMessageTemplateForCC()
                    && promptTemplate->type() == PluginLLMCore::TemplateType::Chat
                ? m_completeSettings.systemPromptForNonFimModels()
                : m_completeSettings.systemPrompt());

    auto project = PluginLLMCore::RulesLoader::getActiveProject();
    if (project) {
        QString projectRules
            = PluginLLMCore::RulesLoader::loadRulesForProject(project, PluginLLMCore::RulesContext::Completions);

        if (!projectRules.isEmpty()) {
            systemPrompt += "\n\n# Project Rules\n\n" + projectRules;
            LOG_MESSAGE("Loaded project rules for completion");
        }
    }

    if (updatedContext.fileContext.has_value())
        systemPrompt.append(updatedContext.fileContext.value());

    if (m_completeSettings.useOpenFilesContext()) {
        if (provider->providerID() == PluginLLMCore::ProviderID::LlamaCpp) {
            for (const auto openedFilePath : m_contextManager->openedFiles({filePath})) {
                if (!updatedContext.filesMetadata) {
                    updatedContext.filesMetadata = QList<PluginLLMCore::FileMetadata>();
                }
                updatedContext.filesMetadata->append({openedFilePath.first, openedFilePath.second});
            }
        } else {
            systemPrompt.append(m_contextManager->openedFilesContext({filePath}));
        }
    }

    updatedContext.systemPrompt = systemPrompt;

    if (promptTemplate->type() == PluginLLMCore::TemplateType::Chat) {
        QString userMessage;
        if (m_completeSettings.useUserMessageTemplateForCC()) {
            userMessage = m_completeSettings.processMessageToFIM(
                updatedContext.prefix.value_or(""), updatedContext.suffix.value_or(""));
        } else {
            userMessage = updatedContext.prefix.value_or("") + updatedContext.suffix.value_or("");
        }

        // TODO refactor add message
        QVector<PluginLLMCore::Message> messages;
        messages.append({"user", userMessage});
        updatedContext.history = messages;
    }

    provider->prepareRequest(
        payload,
        promptTemplate,
        updatedContext,
        PluginLLMCore::RequestType::CodeCompletion,
        false,
        false);

    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestCompleted,
        this,
        &LLMClientInterface::handleFullResponse,
        Qt::UniqueConnection);
    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestFailed,
        this,
        &LLMClientInterface::handleRequestFailed,
        Qt::UniqueConnection);

    auto requestId
        = provider->sendRequest(QUrl(url), payload, resolveEndpoint(promptTemplate, isPreset1Active));
    m_activeRequests[requestId] = {request, provider};
    m_performanceLogger.startTimeMeasurement(requestId);
}

PluginLLMCore::ContextData LLMClientInterface::prepareContext(
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

QString LLMClientInterface::resolveEndpoint(
    PluginLLMCore::PromptTemplate *promptTemplate, bool isLanguageSpecify) const
{
    const QString custom = isLanguageSpecify ? m_generalSettings.ccPreset1CustomEndpoint()
                                             : m_generalSettings.ccCustomEndpoint();
    return !custom.isEmpty() ? custom : promptTemplate->endpoint();
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

    QString outputHandler = m_completeSettings.modelOutputHandler.stringValue();
    QString processedCompletion;

    if (outputHandler == "Raw text") {
        processedCompletion = completion;
    } else if (outputHandler == "Force processing") {
        processedCompletion = CodeHandler::processText(completion,
                                                       Context::extractFilePathFromRequest(request));
    } else { // "Auto"
        processedCompletion = CodeHandler::hasCodeBlocks(completion)
                                  ? CodeHandler::processText(completion,
                                                             Context::extractFilePathFromRequest(
                                                                 request))
                                  : completion;
    }

    if (processedCompletion.endsWith('\n')) {
        QString withoutTrailing = processedCompletion.chopped(1);
        if (!withoutTrailing.contains('\n')) {
            LOG_MESSAGE(QString("Removed trailing newline from single-line completion"));
            processedCompletion = withoutTrailing;
        }
    }

    completionItem[LanguageServerProtocol::textKey] = processedCompletion;
    
    QJsonObject range;
    range["start"] = position;
    range["end"] = position;
    
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
