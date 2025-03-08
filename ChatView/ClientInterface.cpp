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

#include <texteditor/textdocument.h>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include "ChatAssistantSettings.hpp"
#include "ContextManager.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "ProvidersManager.hpp"

namespace QodeAssist::Chat {

ClientInterface::ClientInterface(
    ChatModel *chatModel, LLMCore::IPromptProvider *promptProvider, QObject *parent)
    : QObject(parent)
    , m_requestHandler(new LLMCore::RequestHandler(this))
    , m_chatModel(chatModel)
    , m_promptProvider(promptProvider)
{
    connect(
        m_requestHandler,
        &LLMCore::RequestHandler::completionReceived,
        this,
        [this](const QString &completion, const QJsonObject &request, bool isComplete) {
            handleLLMResponse(completion, request, isComplete);
        });

    connect(
        m_requestHandler,
        &LLMCore::RequestHandler::requestFinished,
        this,
        [this](const QString &, bool success, const QString &errorString) {
            if (!success) {
                emit errorOccurred(errorString);
            }
        });
}

ClientInterface::~ClientInterface() = default;

void ClientInterface::sendMessage(
    const QString &message, const QList<QString> &attachments, const QList<QString> &linkedFiles)
{
    cancelRequest();

    auto attachFiles = Context::ContextManager::instance().getContentFiles(attachments);
    m_chatModel->addMessage(message, ChatModel::ChatRole::User, "", attachFiles);

    auto &chatAssistantSettings = Settings::chatAssistantSettings();

    auto providerName = Settings::generalSettings().caProvider();
    auto provider = LLMCore::ProvidersManager::instance().getProviderByName(providerName);

    if (!provider) {
        LOG_MESSAGE(QString("No provider found with name: %1").arg(providerName));
        return;
    }

    auto templateName = Settings::generalSettings().caTemplate();
    auto promptTemplate = m_promptProvider->getTemplateByName(templateName);

    if (!promptTemplate) {
        LOG_MESSAGE(QString("No template found with name: %1").arg(templateName));
        return;
    }

    LLMCore::ContextData context;

    if (chatAssistantSettings.useSystemPrompt()) {
        QString systemPrompt = chatAssistantSettings.systemPrompt();
        if (!linkedFiles.isEmpty()) {
            systemPrompt = getSystemPromptWithLinkedFiles(systemPrompt, linkedFiles);
        }
        context.systemPrompt = systemPrompt;
    }

    QVector<LLMCore::Message> messages;
    for (const auto &msg : m_chatModel->getChatHistory()) {
        messages.append({msg.role == ChatModel::ChatRole::User ? "user" : "assistant", msg.content});
    }
    context.history = messages;

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Chat;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    if (provider->providerID() == LLMCore::ProviderID::GoogleAI) {
        QString stream = chatAssistantSettings.stream() ? QString{"streamGenerateContent?alt=sse"}
                                                        : QString{"generateContent?"};
        config.url = QUrl(QString("%1/models/%2:%3")
                              .arg(
                                  Settings::generalSettings().caUrl(),
                                  Settings::generalSettings().caModel(),
                                  stream));
    } else {
        config.url
            = QString("%1%2").arg(Settings::generalSettings().caUrl(), provider->chatEndpoint());
        config.providerRequest
            = {{"model", Settings::generalSettings().caModel()},
               {"stream", chatAssistantSettings.stream()}};
    }

    config.apiKey = provider->apiKey();

    config.provider
        ->prepareRequest(config.providerRequest, promptTemplate, context, LLMCore::RequestType::Chat);

    QJsonObject request{{"id", QUuid::createUuid().toString()}};
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

void ClientInterface::handleLLMResponse(
    const QString &response, const QJsonObject &request, bool isComplete)
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
                           .arg(textDocument->mimeType(), textDocument->filePath().toFSPathString());

    QString content = textDocument->document()->toPlainText();

    LOG_MESSAGE(QString("Got context from file: %1").arg(textDocument->filePath().toFSPathString()));

    return QString("Current file context:\n%1\nFile content:\n%2").arg(fileInfo, content);
}

QString ClientInterface::getSystemPromptWithLinkedFiles(
    const QString &basePrompt, const QList<QString> &linkedFiles) const
{
    QString updatedPrompt = basePrompt;

    if (!linkedFiles.isEmpty()) {
        updatedPrompt += "\n\nLinked files for reference:\n";

        auto contentFiles = Context::ContextManager::instance().getContentFiles(linkedFiles);
        for (const auto &file : contentFiles) {
            updatedPrompt += QString("\nFile: %1\nContent:\n%2\n").arg(file.filename, file.content);
        }
    }

    return updatedPrompt;
}

} // namespace QodeAssist::Chat
