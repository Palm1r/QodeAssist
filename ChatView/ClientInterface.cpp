// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ClientInterface.hpp"

#include <memory>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ContentBlocks.hpp>
#include <LLMQore/ToolsManager.hpp>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/target.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QUuid>

#include <ConversationHistory.hpp>
#include <Message.hpp>
#include <ContextRenderer.hpp>
#include <Session.hpp>

#include <QDir>
#include <SessionManager.hpp>
#include <SystemPromptBuilder.hpp>

#include "tools/ReadOriginalHistoryTool.hpp"
#include "tools/TodoTool.hpp"
#include "tools/ToolsRegistration.hpp"

#include "ChatSerializer.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "ProjectSettings.hpp"
#include "SkillsSettings.hpp"
#include "ToolsSettings.hpp"
#include <context/ChangesManager.h>
#include <sources/skills/SkillsManager.hpp>

namespace QodeAssist::Chat {

ClientInterface::ClientInterface(ChatModel *chatModel, QObject *parent)
    : QObject(parent)
    , m_chatModel(chatModel)
    , m_contextManager(new Context::ContextManager(this))
{}

ClientInterface::~ClientInterface()
{
    cancelRequest();
}

void ClientInterface::setSkillsManager(Skills::SkillsManager *skillsManager)
{
    m_skillsManager = skillsManager;
}

void ClientInterface::setSessionManager(SessionManager *sessionManager)
{
    m_sessionManager = sessionManager;
}

void ClientInterface::setActiveAgent(const QString &agentName)
{
    m_activeAgent = agentName;
}

void ClientInterface::setActiveRole(const QString &roleId)
{
    m_activeRoleId = roleId;
}

void ClientInterface::sendMessage(
    const QString &message,
    const QList<QString> &attachments,
    const QList<QString> &linkedFiles)
{
    if (message.trimmed().isEmpty() && attachments.isEmpty()) {
        LOG_MESSAGE("Ignoring empty chat message");
        return;
    }

    cancelRequest();
    m_accumulatedResponses.clear();

    Context::ChangesManager::instance().archiveAllNonArchivedEdits();

    QList<QString> imageFiles;
    QList<QString> textFiles;
    for (const QString &filePath : attachments) {
        if (isImageFile(filePath))
            imageFiles.append(filePath);
        else
            textFiles.append(filePath);
    }

    QList<Context::ContentFile> storedAttachments;
    if (!textFiles.isEmpty() && !m_chatFilePath.isEmpty()) {
        auto attachFiles = m_contextManager->getContentFiles(textFiles);
        for (const auto &file : attachFiles) {
            QString storedPath;
            if (ChatSerializer::saveContentToStorage(
                    m_chatFilePath, file.filename, file.content.toUtf8().toBase64(), storedPath)) {
                Context::ContentFile storedFile;
                storedFile.filename = file.filename;
                storedFile.content = storedPath;
                storedAttachments.append(storedFile);
                LOG_MESSAGE(QString("Stored text file %1 as %2").arg(file.filename, storedPath));
            }
        }
    } else if (!textFiles.isEmpty()) {
        LOG_MESSAGE(QString("Warning: Chat file path not set, cannot save %1 text file(s)")
                        .arg(textFiles.size()));
    }

    QList<ChatModel::ImageAttachment> imageAttachments;
    if (!imageFiles.isEmpty() && !m_chatFilePath.isEmpty()) {
        for (const QString &imagePath : imageFiles) {
            QString base64Data = encodeImageToBase64(imagePath);
            if (base64Data.isEmpty())
                continue;

            QString storedPath;
            QFileInfo fileInfo(imagePath);
            if (ChatSerializer::saveContentToStorage(
                    m_chatFilePath, fileInfo.fileName(), base64Data, storedPath)) {
                ChatModel::ImageAttachment imageAttachment;
                imageAttachment.fileName = fileInfo.fileName();
                imageAttachment.storedPath = storedPath;
                imageAttachment.mediaType = getMediaTypeForImage(imagePath);
                imageAttachments.append(imageAttachment);
                LOG_MESSAGE(QString("Stored image %1 as %2").arg(fileInfo.fileName(), storedPath));
            }
        }
    } else if (!imageFiles.isEmpty()) {
        LOG_MESSAGE(QString("Warning: Chat file path not set, cannot save %1 image(s)")
                        .arg(imageFiles.size()));
    }

    if (!m_sessionManager) {
        const QString error = QStringLiteral("Chat session manager is not available");
        LOG_MESSAGE(error);
        emit errorOccurred(error);
        return;
    }

    // Snapshot prior turns BEFORE the new user message is appended to the model.
    const QVector<ChatModel::Message> priorHistory = m_chatModel->getChatHistory();

    m_chatModel
        ->addMessage(message, ChatModel::ChatRole::User, "", storedAttachments, imageAttachments);

    QString sessionError;
    Session *session = m_sessionManager->createSession(m_activeAgent, &sessionError);
    if (!session) {
        const QString error = sessionError.isEmpty()
                                  ? QStringLiteral("No chat agent selected")
                                  : sessionError;
        LOG_MESSAGE(error);
        emit errorOccurred(error);
        return;
    }

    auto *client = session->client();
    if (!client) {
        const QString error = QStringLiteral("Chat agent has no live client");
        LOG_MESSAGE(error);
        m_sessionManager->removeSession(session);
        emit errorOccurred(error);
        return;
    }

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    Templates::ContextRenderer::Bindings bindings;
    bindings.projectDir = project ? project->projectDirectory().toFSPathString() : QString();
    bindings.homeDir = QDir::homePath();
    bindings.roleId = m_activeRoleId;
    session->setContextBindings(bindings);

    if (m_sessionManager)
        m_sessionManager->toolContributors().contribute(client->tools());
    client->setMaxToolContinuations(Settings::toolsSettings().maxToolContinuations());
    client->setTransferTimeout(
        static_cast<int>(Settings::generalSettings().requestTimeout() * 1000));

    const QString chatContext = buildChatContextLayer(message, linkedFiles);
    if (!chatContext.isEmpty())
        session->systemPrompt()->setLayer(QStringLiteral("chat.context"), chatContext);

    seedHistory(*session->history(), priorHistory);

    QString userText = message;
    if (!storedAttachments.isEmpty() && !m_chatFilePath.isEmpty()) {
        userText += "\n\nAttached files:";
        for (const auto &attachment : storedAttachments) {
            QString fileContent
                = ChatSerializer::loadContentFromStorage(m_chatFilePath, attachment.content);
            if (!fileContent.isEmpty()) {
                QString decoded = QString::fromUtf8(QByteArray::fromBase64(fileContent.toUtf8()));
                userText += QString("\n\nFile: %1\n```\n%2\n```").arg(attachment.filename, decoded);
            }
        }
    }

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    blocks.push_back(std::make_unique<LLMQore::TextContent>(userText));

    if (!imageAttachments.isEmpty() && session->supportsImages() && !m_chatFilePath.isEmpty()) {
        for (const auto &image : imageAttachments) {
            QString base64
                = ChatSerializer::loadContentFromStorage(m_chatFilePath, image.storedPath);
            if (base64.isEmpty())
                continue;
            blocks.push_back(std::make_unique<LLMQore::ImageContent>(
                base64, image.mediaType, LLMQore::ImageContent::ImageSourceType::Base64));
        }
    } else if (!imageAttachments.isEmpty() && !session->supportsImages()) {
        LOG_MESSAGE(QString("Agent '%1' doesn't support images, %2 ignored")
                        .arg(m_activeAgent)
                        .arg(imageAttachments.size()));
    }

    connect(
        client, &::LLMQore::BaseClient::chunkReceived,
        this, &ClientInterface::handlePartialResponse, Qt::UniqueConnection);
    connect(
        client, &::LLMQore::BaseClient::requestCompleted,
        this, &ClientInterface::handleFullResponse, Qt::UniqueConnection);
    connect(
        client, &::LLMQore::BaseClient::requestFinalized,
        this, &ClientInterface::handleRequestFinalized, Qt::UniqueConnection);
    connect(
        client, &::LLMQore::BaseClient::requestFailed,
        this, &ClientInterface::handleRequestFailed, Qt::UniqueConnection);
    connect(
        client, &::LLMQore::BaseClient::toolStarted,
        this, &ClientInterface::handleToolExecutionStarted, Qt::UniqueConnection);
    connect(
        client, &::LLMQore::BaseClient::toolResultReady,
        this, &ClientInterface::handleToolExecutionCompleted, Qt::UniqueConnection);
    connect(
        client, &::LLMQore::BaseClient::thinkingBlockReceived,
        this, &ClientInterface::handleThinkingBlockReceived, Qt::UniqueConnection);

    if (!m_chatFilePath.isEmpty()) {
        if (auto *todoTool
            = qobject_cast<QodeAssist::Tools::TodoTool *>(client->tools()->tool("todo_tool"))) {
            todoTool->setCurrentSessionId(m_chatFilePath);
        }
        if (auto *historyTool = qobject_cast<QodeAssist::Tools::ReadOriginalHistoryTool *>(
                client->tools()->tool("read_original_history"))) {
            historyTool->setCurrentSessionId(m_chatFilePath);
        }
    }

    const LLMQore::RequestID requestId = session->send(std::move(blocks));
    if (requestId.isEmpty()) {
        const QString error = QStringLiteral("Failed to start chat request for agent '%1': %2")
                                  .arg(m_activeAgent, session->lastError().message);
        LOG_MESSAGE(error);
        m_sessionManager->removeSession(session);
        emit errorOccurred(error);
        return;
    }

    QJsonObject request{{"id", requestId}};
    m_activeRequests[requestId] = {request, session};

    emit requestStarted(requestId);
}

void ClientInterface::seedHistory(
    ConversationHistory &history, const QVector<ChatModel::Message> &messages) const
{
    int i = 0;
    while (i < messages.size()) {
        const ChatModel::Message &msg = messages[i];

        if (msg.role == ChatModel::ChatRole::Tool) {
            Message assistant(Message::Role::Assistant);
            Message toolResults(Message::Role::User);
            while (i < messages.size() && messages[i].role == ChatModel::ChatRole::Tool) {
                const ChatModel::Message &toolMsg = messages[i];
                if (!toolMsg.toolName.isEmpty()) {
                    assistant.appendBlock(std::make_unique<LLMQore::ToolUseContent>(
                        toolMsg.id, toolMsg.toolName, toolMsg.toolArguments));
                    toolResults.appendBlock(
                        std::make_unique<LLMQore::ToolResultContent>(toolMsg.id, toolMsg.toolResult));
                }
                ++i;
            }
            if (!assistant.blocks().empty()) {
                history.append(std::move(assistant));
                history.append(std::move(toolResults));
            }
            continue;
        }

        ++i;

        if (msg.role == ChatModel::ChatRole::FileEdit
            || msg.role == ChatModel::ChatRole::Thinking) {
            continue;
        }

        if (msg.role == ChatModel::ChatRole::User) {
            Message userMessage(Message::Role::User);
            QString content = msg.content;
            if (!msg.attachments.isEmpty() && !m_chatFilePath.isEmpty()) {
                content += "\n\nAttached files:";
                for (const auto &attachment : msg.attachments) {
                    QString fileContent = ChatSerializer::loadContentFromStorage(
                        m_chatFilePath, attachment.content);
                    if (!fileContent.isEmpty()) {
                        QString decoded
                            = QString::fromUtf8(QByteArray::fromBase64(fileContent.toUtf8()));
                        content
                            += QString("\n\nFile: %1\n```\n%2\n```").arg(attachment.filename, decoded);
                    }
                }
            }
            userMessage.appendBlock(std::make_unique<LLMQore::TextContent>(content));

            if (!msg.images.isEmpty() && !m_chatFilePath.isEmpty()) {
                for (const auto &image : msg.images) {
                    QString base64 = ChatSerializer::loadContentFromStorage(
                        m_chatFilePath, image.storedPath);
                    if (base64.isEmpty())
                        continue;
                    userMessage.appendBlock(std::make_unique<LLMQore::ImageContent>(
                        base64, image.mediaType, LLMQore::ImageContent::ImageSourceType::Base64));
                }
            }
            history.append(std::move(userMessage));
        } else { // Assistant
            if (msg.content.trimmed().isEmpty())
                continue;
            Message assistant(Message::Role::Assistant);
            assistant.appendBlock(std::make_unique<LLMQore::TextContent>(msg.content));
            history.append(std::move(assistant));
        }
    }
}

QString ClientInterface::buildChatContextLayer(
    const QString &message, const QList<QString> &linkedFiles) const
{
    QString context;

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    if (project) {
        context += QString("# Active project: %1").arg(project->displayName());
        context += QString(
                       "\n# Project source root: %1"
                       "\n#   All new source files, headers, QML and CMake edits MUST be "
                       "created or modified under this directory. Use absolute paths "
                       "rooted here, or project-relative paths.")
                       .arg(project->projectDirectory().toUrlishString());

        if (auto target = project->activeTarget()) {
            if (auto buildConfig = target->activeBuildConfiguration()) {
                context += QString(
                               "\n# Build output directory (compiler artifacts only — do NOT "
                               "create or edit source files here): %1")
                               .arg(buildConfig->buildDirectory().toUrlishString());
            }
        }
    } else {
        context += QString("# No active project in IDE");
    }

    if (m_skillsManager && Settings::skillsSettings().enableSkills()) {
        QStringList projectSkillDirs;
        if (project) {
            Settings::ProjectSettings projectSettings(project);
            projectSkillDirs
                = Settings::SkillsSettings::splitLines(projectSettings.projectSkillDirs());
        }
        m_skillsManager->configure(
            project ? project->projectDirectory().toFSPathString() : QString(),
            Settings::SkillsSettings::splitPaths(Settings::skillsSettings().globalSkillRoots()),
            projectSkillDirs);

        const QString alwaysOnSkills = m_skillsManager->alwaysOnBodies();
        if (!alwaysOnSkills.isEmpty())
            context += QString("\n\n") + alwaysOnSkills;

        const QString skillsCatalog = m_skillsManager->catalogText();
        if (!skillsCatalog.isEmpty())
            context += QString("\n\n") + skillsCatalog;

        static const QRegularExpression skillCommand(
            QStringLiteral("(?:^|\\s)/([a-z0-9][a-z0-9-]*)"));
        QStringList invokedSkillNames;
        auto skillMatch = skillCommand.globalMatch(message);
        while (skillMatch.hasNext()) {
            const QString skillName = skillMatch.next().captured(1);
            if (invokedSkillNames.contains(skillName))
                continue;
            const auto invokedSkill = m_skillsManager->findByName(skillName);
            if (invokedSkill && !invokedSkill->body.isEmpty()) {
                invokedSkillNames << skillName;
                context += QString("\n\n# Invoked Skill: %1\n\n%2")
                               .arg(invokedSkill->name, invokedSkill->body);
            }
        }
    }

    if (!linkedFiles.isEmpty()) {
        context += "\n\nLinked files for reference:\n";
        auto contentFiles = m_contextManager->getContentFiles(linkedFiles);
        for (const auto &file : contentFiles)
            context += QString("\nFile: %1\nContent:\n%2\n").arg(file.filename, file.content);
    }

    return context;
}

void ClientInterface::clearMessages()
{
    m_chatModel->clear();
}

void ClientInterface::cancelRequest()
{
    const auto requests = m_activeRequests;
    m_activeRequests.clear();
    m_accumulatedResponses.clear();
    m_awaitingContinuation.clear();

    for (auto it = requests.begin(); it != requests.end(); ++it) {
        Session *session = it.value().session;
        if (!session)
            continue;
        if (auto *client = session->client())
            disconnect(client, nullptr, this, nullptr);
        if (m_sessionManager)
            m_sessionManager->removeSession(session);
    }

    LOG_MESSAGE("All chat requests cancelled and state cleared");
}

void ClientInterface::handleLLMResponse(const QString &response, const QJsonObject &request)
{
    const auto message = response.trimmed();

    if (!message.isEmpty()) {
        QString messageId = request["id"].toString();
        m_chatModel->addMessage(message, ChatModel::ChatRole::Assistant, messageId);
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

Context::ContextManager *ClientInterface::contextManager() const
{
    return m_contextManager;
}

void ClientInterface::handlePartialResponse(const QString &requestId, const QString &partialText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    if (m_awaitingContinuation.remove(requestId)) {
        m_accumulatedResponses[requestId].clear();
        LOG_MESSAGE(
            QString("Cleared accumulated responses for continuation request %1").arg(requestId));
    }

    m_accumulatedResponses[requestId] += partialText;

    const RequestContext &ctx = it.value();
    handleLLMResponse(m_accumulatedResponses[requestId], ctx.originalRequest);
}

void ClientInterface::handleFullResponse(const QString &requestId, const QString &fullText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const QJsonObject originalRequest = it.value().originalRequest;
    Session *session = it.value().session;

    QString finalText = !fullText.isEmpty() ? fullText : m_accumulatedResponses[requestId];

    QString applyError;
    bool applySuccess
        = Context::ChangesManager::instance().applyPendingEditsForRequest(requestId, &applyError);

    if (!applySuccess) {
        LOG_MESSAGE(QString("Some edits for request %1 were not auto-applied: %2")
                        .arg(requestId, applyError));
    }

    LOG_MESSAGE(
        "Message completed. Final response for message " + originalRequest["id"].toString() + ": "
        + finalText);
    emit messageReceivedCompletely();

    m_activeRequests.erase(it);
    m_accumulatedResponses.remove(requestId);
    m_awaitingContinuation.remove(requestId);

    if (session && m_sessionManager)
        m_sessionManager->removeSession(session);
}

void ClientInterface::handleRequestFinalized(
    const ::LLMQore::RequestID &requestId, const ::LLMQore::CompletionInfo &info)
{
    if (!m_activeRequests.contains(requestId))
        return;
    if (!info.usage)
        return;

    const auto &u = *info.usage;
    m_chatModel->setMessageUsage(
        requestId, u.promptTokens, u.completionTokens, u.cachedPromptTokens, u.reasoningTokens);

    emit messageUsageReceived(
        u.promptTokens, u.completionTokens, u.cachedPromptTokens, u.reasoningTokens);

    LOG_MESSAGE(QString("Chat usage [%1]: prompt=%2 completion=%3 cached=%4 reasoning=%5")
                    .arg(requestId)
                    .arg(u.promptTokens)
                    .arg(u.completionTokens)
                    .arg(u.cachedPromptTokens)
                    .arg(u.reasoningTokens));
}

void ClientInterface::handleRequestFailed(const QString &requestId, const QString &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    Session *session = it.value().session;

    LOG_MESSAGE(QString("Chat request %1 failed: %2").arg(requestId, error));
    emit errorOccurred(error);

    m_activeRequests.erase(it);
    m_accumulatedResponses.remove(requestId);
    m_awaitingContinuation.remove(requestId);

    if (session && m_sessionManager)
        m_sessionManager->removeSession(session);
}

void ClientInterface::handleThinkingBlockReceived(
    const QString &requestId, const QString &thinking, const QString &signature)
{
    if (!m_activeRequests.contains(requestId)) {
        LOG_MESSAGE(QString("Ignoring thinking block for non-chat request: %1").arg(requestId));
        return;
    }

    if (m_awaitingContinuation.remove(requestId)) {
        m_accumulatedResponses[requestId].clear();
        LOG_MESSAGE(
            QString("Cleared accumulated responses for continuation request %1").arg(requestId));
    }

    if (thinking.isEmpty()) {
        m_chatModel->addRedactedThinkingBlock(requestId, signature);
    } else {
        m_chatModel->addThinkingBlock(requestId, thinking, signature);
    }
}

void ClientInterface::handleToolExecutionStarted(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &arguments)
{
    if (!m_activeRequests.contains(requestId)) {
        LOG_MESSAGE(QString("Ignoring tool execution start for non-chat request: %1").arg(requestId));
        return;
    }

    m_chatModel->addToolExecutionStatus(requestId, toolId, toolName, arguments);
    m_awaitingContinuation.insert(requestId);
}

void ClientInterface::handleToolExecutionCompleted(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QString &toolOutput)
{
    if (!m_activeRequests.contains(requestId)) {
        LOG_MESSAGE(QString("Ignoring tool execution result for non-chat request: %1").arg(requestId));
        return;
    }

    m_chatModel->updateToolResult(requestId, toolId, toolName, toolOutput);
}

bool ClientInterface::isImageFile(const QString &filePath) const
{
    static const QSet<QString> imageExtensions = {"png", "jpg", "jpeg", "gif", "webp", "bmp", "svg"};

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    return imageExtensions.contains(extension);
}

QString ClientInterface::getMediaTypeForImage(const QString &filePath) const
{
    static const QHash<QString, QString> mediaTypes
        = {{"png", "image/png"},
           {"jpg", "image/jpeg"},
           {"jpeg", "image/jpeg"},
           {"gif", "image/gif"},
           {"webp", "image/webp"},
           {"bmp", "image/bmp"},
           {"svg", "image/svg+xml"}};

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    if (mediaTypes.contains(extension)) {
        return mediaTypes[extension];
    }

    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    return mimeType.name();
}

QString ClientInterface::encodeImageToBase64(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("Failed to open image file: %1").arg(filePath));
        return QString();
    }

    QByteArray imageData = file.readAll();
    file.close();

    return imageData.toBase64();
}

void ClientInterface::setChatFilePath(const QString &filePath)
{
    m_chatFilePath = filePath;
    m_chatModel->setChatFilePath(filePath);
}

QString ClientInterface::chatFilePath() const
{
    return m_chatFilePath;
}

} // namespace QodeAssist::Chat
