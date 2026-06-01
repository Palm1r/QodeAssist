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

#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <AgentRouter.hpp>
#include <ResponseEvent.hpp>
#include <Session.hpp>
#include <SessionManager.hpp>
#include "sources/common/ContextData.hpp"

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
    SessionManager &sessionManager,
    AgentFactory &agentFactory,
    Context::IDocumentReader &documentReader,
    IRequestPerformanceLogger &performanceLogger)
    : m_generalSettings(generalSettings)
    , m_completeSettings(completeSettings)
    , m_sessionManager(sessionManager)
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

void LLMClientInterface::onSessionEvent(const QString &requestId, const ResponseEvent &event)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    if (event.kind() == ResponseEvent::Kind::TextDelta) {
        if (const auto *delta = event.as<ResponseEvents::TextDelta>())
            it.value().accumulated += delta->text;
    }
}

void LLMClientInterface::onSessionFinished(const QString &requestId)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const QString fullText = it.value().accumulated;
    const QJsonObject originalRequest = it.value().originalRequest;

    sendCompletionToClient(fullText, originalRequest, true);
    finishRequest(requestId);
}

void LLMClientInterface::onSessionFailed(const QString &requestId, const QString &error)
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
        m_sessionManager.removeSession(session);
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
        if (it.value().session)
            m_sessionManager.removeSession(it.value().session);
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

    QString sessionError;
    Session *session = m_sessionManager.createSession(agentName, &sessionError);
    if (!session) {
        LOG_MESSAGE(sessionError);
        sendErrorResponse(request, sessionError);
        return;
    }

    Templates::ContextData context = prepareContext(request, documentInfo);

    QString editorContext;
    if (context.fileContext.has_value())
        editorContext.append(context.fileContext.value());

    if (m_completeSettings.useOpenFilesContext())
        editorContext.append(m_contextManager->openedFilesContext({filePath}));

    if (!editorContext.isEmpty())
        context.systemPrompt = editorContext;

    connect(session, &Session::event, this, [this, session](const ResponseEvent &event) {
        onSessionEvent(requestIdForSession(session), event);
    });
    connect(session, &Session::finished, this, [this, session](const LLMQore::RequestID &, const QString &) {
        onSessionFinished(requestIdForSession(session));
    });
    connect(session, &Session::failed, this, [this, session](const LLMQore::RequestID &, const QString &error) {
        onSessionFailed(requestIdForSession(session), error);
    });

    if (auto *client = session->client())
        client->setTransferTimeout(
            static_cast<int>(m_generalSettings.requestTimeout() * 1000));

    const LLMQore::RequestID requestId = session->sendCompletion(std::move(context));
    if (requestId.isEmpty()) {
        m_sessionManager.removeSession(session);
        QString error = QString("Failed to start completion request for agent: %1").arg(agentName);
        LOG_MESSAGE(error);
        sendErrorResponse(request, error);
        return;
    }

    m_activeRequests[requestId] = {request, session, QString()};
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
    const PluginLLMCore::ContextData legacy
        = reader.prepareContext(lineNumber, cursorPosition, m_completeSettings);

    Templates::ContextData context;
    context.prefix = legacy.prefix;
    context.suffix = legacy.suffix;
    context.fileContext = legacy.fileContext;
    return context;
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
