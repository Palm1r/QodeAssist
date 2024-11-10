/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include <llmcore/RequestConfig.hpp>
#include <texteditor/textdocument.h>

#include "ConfigurationManager.hpp"
#include "DocumentContextReader.hpp"
#include "llmcore/PromptTemplateManager.hpp"
#include "llmcore/ProvidersManager.hpp"
#include "logger/Logger.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/ContextSettings.hpp"
#include "settings/GeneralSettings.hpp"

namespace QodeAssist {

LLMClientInterface::LLMClientInterface()
    : m_requestHandler(this)
{
    connect(&m_requestHandler,
            &LLMCore::RequestHandler::completionReceived,
            this,
            &LLMClientInterface::sendCompletionToClient);
}

Utils::FilePath LLMClientInterface::serverDeviceTemplate() const
{
    return "Qode Assist";
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
        startTimeMeasurement(requestId);
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
    auto updatedContext = prepareContext(request);
    auto &completeSettings = Settings::codeCompletionSettings();

    auto &configManager = ConfigurationManager::instance();
    auto provider = configManager.getCurrentProvider();
    auto promptTemplate = configManager.getCurrentTemplate();

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Fim;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    config.url = QUrl(
        QString("%1%2").arg(Settings::generalSettings().ccUrl(), provider->completionEndpoint()));

    config.providerRequest = {{"model", Settings::generalSettings().ccModel()},
                              {"stream", true},
                              {"stop",
                               QJsonArray::fromStringList(config.promptTemplate->stopWords())}};
    config.multiLineCompletion = completeSettings.multiLineCompletion();

    if (completeSettings.useSystemPrompt())
        config.providerRequest["system"] = completeSettings.systemPrompt();

    config.promptTemplate->prepareRequest(config.providerRequest, updatedContext);
    config.provider->prepareRequest(config.providerRequest, LLMCore::RequestType::Fim);

    m_requestHandler.sendLLMRequest(config, request);
}

LLMCore::ContextData LLMClientInterface::prepareContext(const QJsonObject &request,
                                                        const QStringView &accumulatedCompletion)
{
    QJsonObject params = request["params"].toObject();
    QJsonObject doc = params["doc"].toObject();
    QJsonObject position = doc["position"].toObject();
    QString uri = doc["uri"].toString();

    Utils::FilePath filePath = Utils::FilePath::fromString(QUrl(uri).toLocalFile());
    TextEditor::TextDocument *textDocument = TextEditor::TextDocument::textDocumentForFilePath(
        filePath);

    if (!textDocument) {
        LOG_MESSAGE("Error: Document is not available for" + filePath.toString());
        return LLMCore::ContextData{};
    }

    int cursorPosition = position["character"].toInt();
    int lineNumber = position["line"].toInt();

    DocumentContextReader reader(textDocument);
    return reader.prepareContext(lineNumber, cursorPosition);
}

void LLMClientInterface::sendCompletionToClient(const QString &completion,
                                                const QJsonObject &request,
                                                bool isComplete)
{
    QJsonObject position = request["params"].toObject()["doc"].toObject()["position"].toObject();

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response[LanguageServerProtocol::idKey] = request["id"];
    QJsonObject result;
    QJsonArray completions;
    QJsonObject completionItem;
    completionItem[LanguageServerProtocol::textKey] = completion;
    QJsonObject range;
    range["start"] = position;
    QJsonObject end = position;
    end["character"] = position["character"].toInt() + completion.length();
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

    LOG_MESSAGE(QString("Full response: \n%1")
                   .arg(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Indented))));

    QString requestId = request["id"].toString();
    endTimeMeasurement(requestId);
    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
}

void LLMClientInterface::startTimeMeasurement(const QString &requestId)
{
    m_requestStartTimes[requestId] = QDateTime::currentMSecsSinceEpoch();
}

void LLMClientInterface::endTimeMeasurement(const QString &requestId)
{
    if (m_requestStartTimes.contains(requestId)) {
        qint64 startTime = m_requestStartTimes[requestId];
        qint64 endTime = QDateTime::currentMSecsSinceEpoch();
        qint64 totalTime = endTime - startTime;
        logPerformance(requestId, "TotalCompletionTime", totalTime);
        m_requestStartTimes.remove(requestId);
    }
}

void LLMClientInterface::logPerformance(const QString &requestId,
                                        const QString &operation,
                                        qint64 elapsedMs)
{
    LOG_MESSAGE(QString("Performance: %1 %2 took %3 ms").arg(requestId, operation).arg(elapsedMs));
}

void LLMClientInterface::parseCurrentMessage() {}

} // namespace QodeAssist
