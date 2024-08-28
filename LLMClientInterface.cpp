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

#include <texteditor/textdocument.h>

#include "DocumentContextReader.hpp"
#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistSettings.hpp"
#include "QodeAssistUtils.hpp"

namespace QodeAssist {

LLMClientInterface::LLMClientInterface()
    : m_manager(new QNetworkAccessManager(this))
{
    updateProvider();
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
    updateProvider();

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
        handleCancelRequest(request);
    } else if (method == "exit") {
        // TODO make exit handler
    } else {
        logMessage(QString("Unknown method: %1").arg(method));
    }
}

void LLMClientInterface::handleCancelRequest(const QJsonObject &request)
{
    QString id = request["params"].toObject()["id"].toString();
    if (m_activeRequests.contains(id)) {
        m_activeRequests[id]->abort();
        m_activeRequests.remove(id);
        logMessage(QString("Request %1 cancelled successfully").arg(id));
    } else {
        logMessage(QString("Request %1 not found").arg(id));
    }
}

QString LLMClientInterface::сontextBefore(TextEditor::TextEditorWidget *widget,
                                          int lineNumber,
                                          int cursorPosition)
{
    if (!widget)
        return QString();

    DocumentContextReader reader(widget->textDocument());
    QString languageAndFileInfo = reader.getLanguageAndFileInfo();

    QString contextBefore;
    if (settings().readFullFile()) {
        contextBefore = reader.readWholeFileBefore(lineNumber, cursorPosition);
    } else {
        contextBefore = reader.getContextBefore(lineNumber,
                                                cursorPosition,
                                                settings().readStringsBeforeCursor());
    }

    return QString("%1\n%2\n%3")
        .arg(reader.getSpecificInstructions())
        .arg(reader.getLanguageAndFileInfo())
        .arg(contextBefore);
}

QString LLMClientInterface::сontextAfter(TextEditor::TextEditorWidget *widget,
                                         int lineNumber,
                                         int cursorPosition)
{
    if (!widget)
        return QString();

    DocumentContextReader reader(widget->textDocument());

    QString contextAfter;
    if (settings().readFullFile()) {
        contextAfter = reader.readWholeFileAfter(lineNumber, cursorPosition);
    } else {
        contextAfter = reader.getContextAfter(lineNumber,
                                              cursorPosition,
                                              settings().readStringsAfterCursor());
    }

    return contextAfter;
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
    serverInfo["name"] = "Ollama LSP Server";
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

void LLMClientInterface::handleTextDocumentDidOpen(const QJsonObject &request)
{
}

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

void LLMClientInterface::handleLLMResponse(QNetworkReply *reply, const QJsonObject &request)
{
    QString &accumulatedResponse = m_accumulatedResponses[reply];

    auto &templateManager = PromptTemplateManager::instance();
    const Templates::PromptTemplate *currentTemplate = templateManager.getCurrentTemplate();

    auto &providerManager = LLMProvidersManager::instance();
    bool isComplete = providerManager.getCurrentProvider()->handleResponse(reply,
                                                                           accumulatedResponse);

    QJsonObject position = request["params"].toObject()["doc"].toObject()["position"].toObject();

    if (isComplete || reply->isFinished()) {
        if (isComplete) {
            auto cleanedCompletion = removeStopWords(accumulatedResponse);
            sendCompletionToClient(cleanedCompletion, request, position, true);
        } else {
            handleCompletion(request, accumulatedResponse);
        }
        m_accumulatedResponses.remove(reply);
    }
}

void LLMClientInterface::handleCompletion(const QJsonObject &request,
                                          const QString &accumulatedCompletion)
{
    auto updatedContext = prepareContext(request, accumulatedCompletion);
    sendLLMRequest(request, updatedContext);
}

LLMClientInterface::ContextPair LLMClientInterface::prepareContext(
    const QJsonObject &request, const QString &accumulatedCompletion)
{
    QJsonObject params = request["params"].toObject();
    QJsonObject doc = params["doc"].toObject();
    QJsonObject position = doc["position"].toObject();
    QString uri = doc["uri"].toString();

    Utils::FilePath filePath = Utils::FilePath::fromString(QUrl(uri).toLocalFile());
    TextEditor::TextDocument *textDocument = TextEditor::TextDocument::textDocumentForFilePath(
        filePath);

    if (!textDocument) {
        logMessage("Error: Document is not available for" + filePath.toString());
        return {"", ""};
    }

    int cursorPosition = position["character"].toInt();
    int lineNumber = position["line"].toInt();

    auto textEditor = TextEditor::BaseTextEditor::currentTextEditor();
    TextEditor::TextEditorWidget *widget = textEditor->editorWidget();

    QString contextBefore = сontextBefore(widget, lineNumber, cursorPosition);
    QString contextAfter = сontextAfter(widget, lineNumber, cursorPosition);

    QString updatedContextBefore = contextBefore + accumulatedCompletion;

    return {updatedContextBefore, contextAfter};
}

void LLMClientInterface::updateProvider()
{
    m_serverUrl = QUrl(QString("%1:%2%3")
                           .arg(settings().url.value())
                           .arg(settings().port.value())
                           .arg(settings().endPoint.value()));
}

void LLMClientInterface::sendCompletionToClient(const QString &completion,
                                                const QJsonObject &request,
                                                const QJsonObject &position,
                                                bool isComplete)
{
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

    logMessage(
        QString("Completions: \n%1")
            .arg(QString::fromUtf8(QJsonDocument(completions).toJson(QJsonDocument::Indented))));

    logMessage(QString("Full response: \n%1")
                   .arg(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Indented))));

    emit messageReceived(LanguageServerProtocol::JsonRpcMessage(response));
}

void LLMClientInterface::sendLLMRequest(const QJsonObject &request, const ContextPair &prompt)
{
    QJsonObject ollamaRequest = {{"model", settings().modelName.value()}, {"stream", true}};

    auto currentTemplate = PromptTemplateManager::instance().getCurrentTemplate();
    currentTemplate->prepareRequest(ollamaRequest, prompt.prefix, prompt.suffix);

    auto &providerManager = LLMProvidersManager::instance();
    providerManager.getCurrentProvider()->prepareRequest(ollamaRequest);

    logMessage(
        QString("Sending request to llm: \nurl: %1\nRequest body:\n%2")
            .arg(m_serverUrl.toString())
            .arg(QString::fromUtf8(QJsonDocument(ollamaRequest).toJson(QJsonDocument::Indented))));

    QNetworkRequest networkRequest(m_serverUrl);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_manager->post(networkRequest, QJsonDocument(ollamaRequest).toJson());
    if (!reply) {
        logMessage("Error: Failed to create network reply");
        return;
    }

    QString requestId = request["id"].toString();
    m_activeRequests[requestId] = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, request]() {
        handleLLMResponse(reply, request);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        reply->deleteLater();
        m_activeRequests.remove(requestId);
        if (reply->error() != QNetworkReply::NoError) {
            logMessage(QString("Error in Ollama request: %1").arg(reply->errorString()));
        } else {
            logMessage("Request finished successfully");
        }
    });
}

QString LLMClientInterface::removeStopWords(const QString &completion)
{
    QString filteredCompletion = completion;

    auto currentTemplate = PromptTemplateManager::instance().getCurrentTemplate();
    QStringList stopWords = currentTemplate->stopWords();

    for (const QString &stopWord : stopWords) {
        filteredCompletion = filteredCompletion.replace(stopWord, "");
    }

    return filteredCompletion;
}

void LLMClientInterface::parseCurrentMessage()
{
}

} // namespace QodeAssist
