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

#include "CodeHandler.hpp"
#include "context/DocumentContextReader.hpp"
#include "llmcore/MessageBuilder.hpp"
#include "llmcore/PromptTemplateManager.hpp"
#include "llmcore/ProvidersManager.hpp"
#include "logger/Logger.hpp"
#include "settings/CodeCompletionSettings.hpp"
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

bool QodeAssist::LLMClientInterface::isSpecifyCompletion(const QJsonObject &request)
{
    auto &generalSettings = Settings::generalSettings();

    Context::ProgrammingLanguage documentLanguage = getDocumentLanguage(request);
    Context::ProgrammingLanguage preset1Language = Context::ProgrammingLanguageUtils::fromString(
        generalSettings.preset1Language.displayForIndex(generalSettings.preset1Language()));

    return generalSettings.specifyPreset1() && documentLanguage == preset1Language;
}

void LLMClientInterface::handleCompletion(const QJsonObject &request)
{
    const auto updatedContext = prepareContext(request);
    auto &completeSettings = Settings::codeCompletionSettings();
    auto &generalSettings = Settings::generalSettings();

    bool isPreset1Active = isSpecifyCompletion(request);

    const auto providerName = !isPreset1Active ? generalSettings.ccProvider()
                                               : generalSettings.ccPreset1Provider();
    const auto modelName = !isPreset1Active ? generalSettings.ccModel()
                                            : generalSettings.ccPreset1Model();
    const auto url = !isPreset1Active ? generalSettings.ccUrl() : generalSettings.ccPreset1Url();

    const auto provider = LLMCore::ProvidersManager::instance().getProviderByName(providerName);

    if (!provider) {
        LOG_MESSAGE(QString("No provider found with name: %1").arg(providerName));
        return;
    }

    auto templateName = !isPreset1Active ? generalSettings.ccTemplate()
                                         : generalSettings.ccPreset1Template();

    auto promptTemplate = LLMCore::PromptTemplateManager::instance().getFimTemplateByName(
        templateName);

    if (!promptTemplate) {
        LOG_MESSAGE(QString("No template found with name: %1").arg(templateName));
        return;
    }

    // TODO refactor to dynamic presets system
    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::CodeCompletion;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    config.url = QUrl(QString("%1%2").arg(
        url,
        promptTemplate->type() == LLMCore::TemplateType::Fim ? provider->completionEndpoint()
                                                             : provider->chatEndpoint()));
    config.apiKey = provider->apiKey();

    config.providerRequest = {{"model", modelName}, {"stream", completeSettings.stream()}};

    config.multiLineCompletion = completeSettings.multiLineCompletion();

    const auto stopWords = QJsonArray::fromStringList(config.promptTemplate->stopWords());
    if (!stopWords.isEmpty())
        config.providerRequest["stop"] = stopWords;

    QString systemPrompt;
    if (completeSettings.useSystemPrompt())
        systemPrompt.append(completeSettings.systemPrompt());
    if (!updatedContext.fileContext.isEmpty())
        systemPrompt.append(updatedContext.fileContext);

    QString userMessage;
    if (completeSettings.useUserMessageTemplateForCC() && promptTemplate->type() == LLMCore::TemplateType::Chat) {
        userMessage = completeSettings.userMessageTemplateForCC().arg(updatedContext.prefix, updatedContext.suffix);
    } else {
        userMessage = updatedContext.prefix;
    }

    auto message = LLMCore::MessageBuilder()
                       .addSystemMessage(systemPrompt)
                       .addUserMessage(userMessage)
                       .addSuffix(updatedContext.suffix)
                       .addTokenizer(promptTemplate);

    message.saveTo(
        config.providerRequest,
        providerName == "Ollama" ? LLMCore::ProvidersApi::Ollama : LLMCore::ProvidersApi::OpenAI);

    config.provider->prepareRequest(config.providerRequest, LLMCore::RequestType::CodeCompletion);

    auto errors = config.provider->validateRequest(config.providerRequest, promptTemplate->type());
    if (!errors.isEmpty()) {
        LOG_MESSAGE("Validate errors for fim request:");
        LOG_MESSAGES(errors);
        return;
    }
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

    Context::DocumentContextReader reader(textDocument);
    return reader.prepareContext(lineNumber, cursorPosition);
}

Context::ProgrammingLanguage LLMClientInterface::getDocumentLanguage(const QJsonObject &request) const
{
    QJsonObject params = request["params"].toObject();
    QJsonObject doc = params["doc"].toObject();
    QString uri = doc["uri"].toString();

    Utils::FilePath filePath = Utils::FilePath::fromString(QUrl(uri).toLocalFile());
    TextEditor::TextDocument *textDocument = TextEditor::TextDocument::textDocumentForFilePath(
        filePath);

    if (!textDocument) {
        LOG_MESSAGE("Error: Document is not available for" + filePath.toString());
        return Context::ProgrammingLanguage::Unknown;
    }

    return Context::ProgrammingLanguageUtils::fromMimeType(textDocument->mimeType());
}

void LLMClientInterface::sendCompletionToClient(const QString &completion,
                                                const QJsonObject &request,
                                                bool isComplete)
{
    bool isPreset1Active = isSpecifyCompletion(request);

    auto templateName = !isPreset1Active ? Settings::generalSettings().ccTemplate()
                                         : Settings::generalSettings().ccPreset1Template();

    auto promptTemplate = LLMCore::PromptTemplateManager::instance().getFimTemplateByName(
        templateName);

    QJsonObject position = request["params"].toObject()["doc"].toObject()["position"].toObject();

    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response[LanguageServerProtocol::idKey] = request["id"];

    QJsonObject result;
    QJsonArray completions;
    QJsonObject completionItem;

    QString processedCompletion
        = promptTemplate->type() == LLMCore::TemplateType::Chat
                  && Settings::codeCompletionSettings().smartProcessInstuctText()
              ? CodeHandler::processText(completion)
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
