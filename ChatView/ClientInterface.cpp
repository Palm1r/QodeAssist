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

#include "ClientInterface.hpp"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <texteditor/textdocument.h>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include "ChatAssistantSettings.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "PromptTemplateManager.hpp"
#include "ProvidersManager.hpp"

namespace QodeAssist::Chat {

ClientInterface::ClientInterface(ChatModel *chatModel, QObject *parent)
    : QObject(parent)
    , m_requestHandler(new LLMCore::RequestHandler(this))
    , m_chatModel(chatModel)
{
    connect(m_requestHandler,
            &LLMCore::RequestHandler::completionReceived,
            this,
            [this](const QString &completion, const QJsonObject &request, bool isComplete) {
                handleLLMResponse(completion, request, isComplete);
            });

    connect(m_requestHandler,
            &LLMCore::RequestHandler::requestFinished,
            this,
            [this](const QString &, bool success, const QString &errorString) {
                if (!success) {
                    emit errorOccurred(errorString);
                }
            });
}

ClientInterface::~ClientInterface() = default;

void ClientInterface::sendMessage(const QString &message, bool includeCurrentFile)
{
    cancelRequest();

    m_chatModel->addMessage(message, ChatModel::ChatRole::User, "");

    auto &chatAssistantSettings = Settings::chatAssistantSettings();

    auto providerName = Settings::generalSettings().caProvider();
    auto provider = LLMCore::ProvidersManager::instance().getProviderByName(providerName);

    if (!provider) {
        LOG_MESSAGE(QString("No provider found with name: %1").arg(providerName));
        return;
    }

    auto templateName = Settings::generalSettings().caTemplate();
    auto promptTemplate = LLMCore::PromptTemplateManager::instance().getChatTemplateByName(
        templateName);

    if (!promptTemplate) {
        LOG_MESSAGE(QString("No template found with name: %1").arg(templateName));
        return;
    }

    LLMCore::ContextData context;
    context.prefix = message;
    context.suffix = "";

    QString systemPrompt;
    if (chatAssistantSettings.useSystemPrompt())
        systemPrompt = chatAssistantSettings.systemPrompt();

    if (includeCurrentFile) {
        QString fileContext = getCurrentFileContext();
        if (!fileContext.isEmpty()) {
            systemPrompt = systemPrompt.append(fileContext);
        }
    }

    QJsonObject providerRequest;
    providerRequest["model"] = Settings::generalSettings().caModel();
    providerRequest["stream"] = chatAssistantSettings.stream();
    providerRequest["messages"] = m_chatModel->prepareMessagesForRequest(systemPrompt);

    if (promptTemplate)
        promptTemplate->prepareRequest(providerRequest, context);
    else
        qWarning("No prompt template found");

    if (provider)
        provider->prepareRequest(providerRequest, LLMCore::RequestType::Chat);
    else
        qWarning("No provider found");

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Chat;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    config.url = QString("%1%2").arg(Settings::generalSettings().caUrl(), provider->chatEndpoint());
    config.providerRequest = providerRequest;
    config.multiLineCompletion = false;
    config.apiKey = provider->apiKey();

    QJsonObject request;
    request["id"] = QUuid::createUuid().toString();

    auto errors = config.provider->validateRequest(config.providerRequest, promptTemplate->type());
    if (!errors.isEmpty()) {
        LOG_MESSAGE("Validate errors for chat request:");
        LOG_MESSAGES(errors);
        return;
    }

    m_requestHandler->sendLLMRequest(config, request);
}

void ClientInterface::clearMessages()
{
    m_chatModel->clear();
    LOG_MESSAGE("Chat history cleared");
}

void ClientInterface::cancelRequest()
{
    auto id = m_chatModel->lastMessageId();
    m_requestHandler->cancelRequest(id);
}

void ClientInterface::handleLLMResponse(const QString &response,
                                        const QJsonObject &request,
                                        bool isComplete)
{
    const auto message = response.trimmed();

    if (!message.isEmpty()) {
        QString messageId = request["id"].toString();
        m_chatModel->addMessage(message, ChatModel::ChatRole::Assistant, messageId);

        if (isComplete) {
            LOG_MESSAGE(
                "Message completed. Final response for message " + messageId + ": " + response);
            emit messageReceivedCompletely();
        }
    }
}

QString ClientInterface::getCurrentFileContext() const
{
    auto currentEditor = Core::EditorManager::currentEditor();
    if (!currentEditor) {
        LOG_MESSAGE("No active editor found");
        return QString();
    }

    auto textDocument = qobject_cast<TextEditor::TextDocument *>(currentEditor->document());
    if (!textDocument) {
        LOG_MESSAGE("Current document is not a text document");
        return QString();
    }

    QString fileInfo = QString("Language: %1\nFile: %2\n\n")
                           .arg(textDocument->mimeType(), textDocument->filePath().toString());

    QString content = textDocument->document()->toPlainText();

    LOG_MESSAGE(QString("Got context from file: %1").arg(textDocument->filePath().toString()));

    return QString("Current file context:\n%1\nFile content:\n%2").arg(fileInfo, content);
}

} // namespace QodeAssist::Chat
