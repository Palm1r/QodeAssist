// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ClientInterface.hpp"

#include <memory>
#include <vector>

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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QUuid>

#include <ConversationHistory.hpp>
#include <ContextRenderer.hpp>
#include <Message.hpp>
#include <PluginBlocks.hpp>
#include <Session.hpp>
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

namespace {
struct StoredImage
{
    QString fileName;
    QString storedPath;
    QString mediaType;
};
} // namespace

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

void ClientInterface::setHistory(ConversationHistory *history)
{
    m_history = history;
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

    QList<StoredImage> storedImages;
    if (!imageFiles.isEmpty() && !m_chatFilePath.isEmpty()) {
        for (const QString &imagePath : imageFiles) {
            QString base64Data = encodeImageToBase64(imagePath);
            if (base64Data.isEmpty())
                continue;

            QString storedPath;
            QFileInfo fileInfo(imagePath);
            if (ChatSerializer::saveContentToStorage(
                    m_chatFilePath, fileInfo.fileName(), base64Data, storedPath)) {
                storedImages.append(
                    {fileInfo.fileName(), storedPath, getMediaTypeForImage(imagePath)});
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
    if (!m_history) {
        const QString error = QStringLiteral("Chat history is not available");
        LOG_MESSAGE(error);
        emit errorOccurred(error);
        return;
    }

    QString sessionError;
    Session *session = m_sessionManager->createSession(m_activeAgent, m_history, &sessionError);
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

    const QString chatFilePath = m_chatFilePath;
    session->setContentLoader([chatFilePath](const QString &storedPath) {
        return ChatSerializer::loadContentFromStorage(chatFilePath, storedPath);
    });

    m_sessionManager->toolContributors().contribute(client->tools());
    client->setMaxToolContinuations(Settings::toolsSettings().maxToolContinuations());
    client->setTransferTimeout(
        static_cast<int>(Settings::generalSettings().requestTimeout() * 1000));

    const QString chatContext = buildChatContextLayer(message, linkedFiles);
    if (!chatContext.isEmpty())
        session->systemPrompt()->setLayer(QStringLiteral("chat.context"), chatContext);

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    blocks.push_back(std::make_unique<LLMQore::TextContent>(message));

    for (const auto &attachment : storedAttachments) {
        blocks.push_back(
            std::make_unique<StoredAttachmentContent>(attachment.filename, attachment.content));
    }

    if (!storedImages.isEmpty() && session->supportsImages()) {
        for (const auto &image : storedImages) {
            blocks.push_back(std::make_unique<StoredImageContent>(
                image.fileName, image.storedPath, image.mediaType));
        }
    } else if (!storedImages.isEmpty() && !session->supportsImages()) {
        LOG_MESSAGE(QString("Agent '%1' doesn't support images, %2 ignored")
                        .arg(m_activeAgent)
                        .arg(storedImages.size()));
    }

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

    connect(session, &Session::event, this, [this, session](const QodeAssist::ResponseEvent &ev) {
        onSessionEvent(session, ev);
    });
    connect(
        session, &Session::finished, this,
        [this](const LLMQore::RequestID &id, const QString &) { onSessionFinished(id); });
    connect(
        session, &Session::failed, this,
        [this](const LLMQore::RequestID &id, const QodeAssist::ErrorInfo &error) {
            onSessionFailed(id, error);
        });

    const LLMQore::RequestID requestId = session->send(std::move(blocks));
    if (requestId.isEmpty()) {
        const QString error = QStringLiteral("Failed to start chat request for agent '%1': %2")
                                  .arg(m_activeAgent, session->lastError().message);
        LOG_MESSAGE(error);
        m_sessionManager->removeSession(session);
        emit errorOccurred(error);
        return;
    }

    m_activeRequests[requestId] = {QJsonObject{{"id", requestId}}, session};

    emit requestStarted(requestId);
}

QString ClientInterface::requestIdForSession(Session *session) const
{
    for (auto it = m_activeRequests.cbegin(); it != m_activeRequests.cend(); ++it) {
        if (it.value().session == session)
            return it.key();
    }
    return {};
}

void ClientInterface::onSessionEvent(Session *session, const QodeAssist::ResponseEvent &ev)
{
    if (ev.kind() != ResponseEvent::Kind::Usage)
        return;

    const auto *usage = ev.as<ResponseEvents::Usage>();
    if (!usage)
        return;

    const QString requestId = requestIdForSession(session);
    if (!requestId.isEmpty()) {
        m_chatModel->setMessageUsage(
            requestId,
            usage->inputTokens,
            usage->outputTokens,
            usage->cachedTokens,
            usage->reasoningTokens);
    }

    emit messageUsageReceived(
        usage->inputTokens, usage->outputTokens, usage->cachedTokens, usage->reasoningTokens);

    LOG_MESSAGE(QString("Chat usage [%1]: prompt=%2 completion=%3 cached=%4 reasoning=%5")
                    .arg(requestId)
                    .arg(usage->inputTokens)
                    .arg(usage->outputTokens)
                    .arg(usage->cachedTokens)
                    .arg(usage->reasoningTokens));
}

void ClientInterface::onSessionFinished(const QString &requestId)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    Session *session = it.value().session;

    QString applyError;
    if (!Context::ChangesManager::instance().applyPendingEditsForRequest(requestId, &applyError)) {
        LOG_MESSAGE(QString("Some edits for request %1 were not auto-applied: %2")
                        .arg(requestId, applyError));
    }

    emit messageReceivedCompletely();

    m_activeRequests.erase(it);

    if (session && m_sessionManager)
        m_sessionManager->removeSession(session);
}

void ClientInterface::onSessionFailed(const QString &requestId, const QodeAssist::ErrorInfo &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    Session *session = it.value().session;

    LOG_MESSAGE(QString("Chat request %1 failed: %2").arg(requestId, error.message));
    emit errorOccurred(error.message);

    m_activeRequests.erase(it);

    if (session && m_sessionManager)
        m_sessionManager->removeSession(session);
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
    if (m_history)
        m_history->clear();
}

void ClientInterface::cancelRequest()
{
    const auto requests = m_activeRequests;
    m_activeRequests.clear();

    for (auto it = requests.begin(); it != requests.end(); ++it) {
        Session *session = it.value().session;
        if (session && m_sessionManager)
            m_sessionManager->removeSession(session);
    }

    LOG_MESSAGE("All chat requests cancelled and state cleared");
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
