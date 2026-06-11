// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "LLMClientInterface.hpp"

#include <LLMQore/BaseClient.hpp>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <utils/filepath.h>

#include <Agent.hpp>
#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <AgentRouter.hpp>
#include <ConversationHistory.hpp>
#include <PluginBlocks.hpp>
#include <Session.hpp>
#include <SystemPromptBuilder.hpp>
#include "sources/common/ContextData.hpp"

#include <LLMQore/ContentBlocks.hpp>

#include <memory>
#include <vector>

#include "CodeHandler.hpp"
#include "context/DocumentContextReader.hpp"
#include "context/Utils.hpp"
#include "logger/Logger.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "sources/settings/PipelinesConfig.hpp"

namespace QodeAssist {

LLMClientInterface::LLMClientInterface(
    const Settings::GeneralSettings &generalSettings,
    const Settings::CodeCompletionSettings &completeSettings,
    AgentFactory &agentFactory,
    Context::IDocumentReader &documentReader,
    IRequestPerformanceLogger &performanceLogger)
    : m_generalSettings(generalSettings)
    , m_completeSettings(completeSettings)
    , m_agentFactory(agentFactory)
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

void LLMClientInterface::onCompletionFinished(const QString &requestId)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    QString fullText;
    if (Session *session = it.value().session) {
        if (auto *history = session->history(); history && !history->isEmpty())
            fullText = history->messages().back().text();
    }
    const QJsonObject originalRequest = it.value().originalRequest;

    sendCompletionToClient(fullText, originalRequest, true);
    finishRequest(requestId);
}

void LLMClientInterface::onCompletionFailed(const QString &requestId, const QString &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    LOG_MESSAGE(QString("Request %1 failed: %2").arg(requestId, error));

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response[LanguageServerProtocol::idKey] = it.value().originalRequest["id"];

    QJsonObject errorObject;
    errorObject["code"] = -32603; // Internal error code
    errorObject["message"] = error;
    response["error"] = errorObject;

    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
    finishRequest(requestId);
}

void LLMClientInterface::finishRequest(const QString &requestId)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    Session *session = it.value().session;
    m_activeRequests.erase(it);
    m_performanceLogger.endTimeMeasurement(requestId);

    if (session)
        session->deleteLater();
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
    const auto requests = m_activeRequests;
    m_activeRequests.clear();

    for (auto it = requests.begin(); it != requests.end(); ++it) {
        m_performanceLogger.endTimeMeasurement(it.key());
        if (Session *session = it.value().session) {
            session->cancel();
            session->deleteLater();
        }
    }

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

    const QString agentName = pickCompletionAgent(filePath);
    if (agentName.isEmpty()) {
        QString error = QString("No code completion agent matches: %1").arg(filePath);
        LOG_MESSAGE(error);
        sendErrorResponse(request, error);
        return;
    }

    QString agentError;
    Agent *agent = m_agentFactory.create(agentName, /*parent=*/nullptr, &agentError);
    if (!agent) {
        LOG_MESSAGE(agentError);
        sendErrorResponse(request, agentError);
        return;
    }

    auto *session = new Session(agent, this);
    if (!session->isValid()) {
        const QString error = session->invalidReason();
        delete session;
        LOG_MESSAGE(error);
        sendErrorResponse(request, error);
        return;
    }

    Templates::ContextData context = prepareContext(request, documentInfo);

    QString editorContext;
    if (context.fileContext.has_value())
        editorContext.append(context.fileContext.value());

    if (m_completeSettings.useOpenFilesContext())
        editorContext.append(m_contextManager->openedFilesContext({filePath}));

    if (!editorContext.isEmpty())
        session->systemPrompt()->setLayer(QStringLiteral("completion.context"), editorContext);

    connect(session, &Session::finished, this, [this, session](const LLMQore::RequestID &, const QString &) {
        onCompletionFinished(requestIdForSession(session));
    });
    connect(session, &Session::failed, this, [this, session](const LLMQore::RequestID &, const QodeAssist::ErrorInfo &error) {
        onCompletionFailed(requestIdForSession(session), error.message);
    });

    if (auto *client = session->client())
        client->setTransferTimeout(
            static_cast<int>(m_generalSettings.requestTimeout() * 1000));

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    blocks.push_back(std::make_unique<CompletionContent>(
        context.prefix.value_or(QString()), context.suffix.value_or(QString())));
    const LLMQore::RequestID requestId = session->send(std::move(blocks), /*toolsOverride=*/false);
    if (requestId.isEmpty()) {
        QString error = QString("Failed to start completion request for agent '%1': %2")
                            .arg(agentName, session->lastError().message);
        session->deleteLater();
        LOG_MESSAGE(error);
        sendErrorResponse(request, error);
        return;
    }

    m_activeRequests[requestId] = {request, session};
    m_performanceLogger.startTimeMeasurement(requestId);
}

QString LLMClientInterface::pickCompletionAgent(const QString &filePath) const
{
    const QStringList roster = Settings::PipelinesConfig::load().rosters.codeCompletion;
    if (roster.isEmpty())
        return {};

    AgentRouter::Context ctx;
    ctx.filePath = filePath;
    if (auto *project = ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(filePath)))
        ctx.projectName = project->displayName();

    return AgentRouter::pickAgent(roster, ctx, m_agentFactory);
}

QString LLMClientInterface::requestIdForSession(Session *session) const
{
    for (auto it = m_activeRequests.cbegin(); it != m_activeRequests.cend(); ++it) {
        if (it.value().session == session)
            return it.key();
    }
    return {};
}

Templates::ContextData LLMClientInterface::prepareContext(
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
