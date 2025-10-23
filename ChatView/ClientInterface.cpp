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

#include "ClientInterface.hpp"

#include <texteditor/textdocument.h>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectmanager.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include "ChatAssistantSettings.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "ProvidersManager.hpp"
#include "RequestConfig.hpp"
#include <RulesLoader.hpp>

namespace QodeAssist::Chat {

ClientInterface::ClientInterface(
    ChatModel *chatModel, LLMCore::IPromptProvider *promptProvider, QObject *parent)
    : QObject(parent)
    , m_chatModel(chatModel)
    , m_promptProvider(promptProvider)
    , m_contextManager(new Context::ContextManager(this))
{}

ClientInterface::~ClientInterface() = default;

void ClientInterface::sendMessage(
    const QString &message, const QList<QString> &attachments, const QList<QString> &linkedFiles)
{
    cancelRequest();
    m_accumulatedResponses.clear();

    auto attachFiles = m_contextManager->getContentFiles(attachments);
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

        if (Settings::generalSettings().useTools()) {
            systemPrompt
                += "\n\n# Smart Tool Combinations\n\n"
                   "**Understanding code structure:**\n"
                   "find_cpp_symbol → read_project_file_by_path → find_cpp_symbol (for related "
                   "symbols)\n\n"
                   "**Finding usages and references:**\n"
                   "find_cpp_symbol (declaration) → search_in_project (with precise name) → read "
                   "files\n\n"
                   "**Fixing compilation errors:**\n"
                   "get_issues_list → find_cpp_symbol (error location) → read_project_file_by_path "
                   "→ edit_project_file\n\n"
                   "**Complex refactoring:**\n"
                   "find_cpp_symbol (all targets) → read files → search_in_project (verify "
                   "coverage) → edit files\n\n"
                   "**Adding features to existing code:**\n"
                   "find_cpp_symbol (target class/namespace) → read_project_file_by_path → "
                   "edit_project_file\n\n"
                   "# Best Practices\n\n"
                   "- **Prefer semantic over text search**: Use find_cpp_symbol before "
                   "search_in_project\n"
                   "- **Make atomic edits**: One comprehensive change instead of multiple small "
                   "ones\n"
                   "- **Read once, understand fully**: Avoid re-reading the same file multiple "
                   "times\n"
                   "- **Start with visible context**: Use read_visible_files when user references "
                   "current work\n"
                   "- **Verify before editing**: Always read and understand code before proposing "
                   "changes\n"
                   "- **Use file patterns**: Narrow search_in_project with *.cpp, *.h patterns\n"
                   "- **Chain intelligently**: Each tool result should inform the next tool "
                   "choice\n"
                   "- **Fix related issues together**: When fixing errors, address all related "
                   "problems in one edit\n";
        }

        auto project = LLMCore::RulesLoader::getActiveProject();
        if (project) {
            QString projectRules
                = LLMCore::RulesLoader::loadRulesForProject(project, LLMCore::RulesContext::Chat);

            if (!projectRules.isEmpty()) {
                systemPrompt += "\n# Project Rules\n\n" + projectRules;
            }
        }

        if (!linkedFiles.isEmpty()) {
            systemPrompt = getSystemPromptWithLinkedFiles(systemPrompt, linkedFiles);
        }
        context.systemPrompt = systemPrompt;
    }

    QVector<LLMCore::Message> messages;
    for (const auto &msg : m_chatModel->getChatHistory()) {
        if (msg.role == ChatModel::ChatRole::Tool || msg.role == ChatModel::ChatRole::FileEdit) {
            continue;
        }
        messages.append({msg.role == ChatModel::ChatRole::User ? "user" : "assistant", msg.content});
    }
    context.history = messages;

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Chat;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    if (provider->providerID() == LLMCore::ProviderID::GoogleAI) {
        QString stream = QString{"streamGenerateContent?alt=sse"};
        config.url = QUrl(QString("%1/models/%2:%3")
                              .arg(
                                  Settings::generalSettings().caUrl(),
                                  Settings::generalSettings().caModel(),
                                  stream));
    } else {
        config.url
            = QString("%1%2").arg(Settings::generalSettings().caUrl(), provider->chatEndpoint());
        config.providerRequest
            = {{"model", Settings::generalSettings().caModel()}, {"stream", true}};
    }

    config.apiKey = provider->apiKey();

    config.provider
        ->prepareRequest(config.providerRequest, promptTemplate, context, LLMCore::RequestType::Chat);

    QString requestId = QUuid::createUuid().toString();
    QJsonObject request{{"id", requestId}};

    m_activeRequests[requestId] = {request, provider};

    connect(
        provider,
        &LLMCore::Provider::partialResponseReceived,
        this,
        &ClientInterface::handlePartialResponse,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::fullResponseReceived,
        this,
        &ClientInterface::handleFullResponse,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::requestFailed,
        this,
        &ClientInterface::handleRequestFailed,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::toolExecutionStarted,
        m_chatModel,
        &ChatModel::addToolExecutionStatus,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::toolExecutionCompleted,
        m_chatModel,
        &ChatModel::updateToolResult,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::continuationStarted,
        this,
        &ClientInterface::handleCleanAccumulatedData,
        Qt::UniqueConnection);

    provider->sendRequest(requestId, config.url, config.providerRequest);
}

void ClientInterface::clearMessages()
{
    m_chatModel->clear();
    LOG_MESSAGE("Chat history cleared");
}

void ClientInterface::cancelRequest()
{
    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        const RequestContext &ctx = it.value();
        if (ctx.provider) {
            ctx.provider->cancelRequest(it.key());
        }
    }

    m_activeRequests.clear();
    m_accumulatedResponses.clear();

    LOG_MESSAGE("All requests cancelled and state cleared");
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

        auto contentFiles = m_contextManager->getContentFiles(linkedFiles);
        for (const auto &file : contentFiles) {
            updatedPrompt += QString("\nFile: %1\nContent:\n%2\n").arg(file.filename, file.content);
        }
    }

    return updatedPrompt;
}

Context::ContextManager *ClientInterface::contextManager() const
{
    return m_contextManager;
}

void ClientInterface::handlePartialResponse(const QString &requestId, const QString &partialText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    m_accumulatedResponses[requestId] += partialText;

    const RequestContext &ctx = it.value();
    handleLLMResponse(m_accumulatedResponses[requestId], ctx.originalRequest, false);
}

void ClientInterface::handleFullResponse(const QString &requestId, const QString &fullText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const RequestContext &ctx = it.value();

    QString finalText = !fullText.isEmpty() ? fullText : m_accumulatedResponses[requestId];
    handleLLMResponse(finalText, ctx.originalRequest, true);

    m_activeRequests.erase(it);
    m_accumulatedResponses.remove(requestId);
}

void ClientInterface::handleRequestFailed(const QString &requestId, const QString &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    LOG_MESSAGE(QString("Chat request %1 failed: %2").arg(requestId, error));
    emit errorOccurred(error);

    m_activeRequests.erase(it);
    m_accumulatedResponses.remove(requestId);
}

void ClientInterface::handleCleanAccumulatedData(const QString &requestId)
{
    m_accumulatedResponses[requestId].clear();
    LOG_MESSAGE(QString("Cleared accumulated responses for continuation request %1").arg(requestId));
}

} // namespace QodeAssist::Chat
