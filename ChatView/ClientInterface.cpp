// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ClientInterface.hpp"

#include <memory>
#include <vector>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ContentBlocks.hpp>
#include <LLMQore/ToolLoopRunner.hpp>
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

#include <AgentFactory.hpp>
#include <ContextRenderer.hpp>
#include <ConversationHistory.hpp>
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
#include <context/EnvBlockFormatter.hpp>
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
    , m_contentCache(std::make_shared<StoredContentCache>())
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

Session *ClientInterface::session() const
{
    return m_session;
}

void ClientInterface::ensureSession()
{
    if (m_session || !m_sessionManager || !m_history)
        return;

    m_session = m_sessionManager->createDetachedSession(m_history, this);

    connect(m_session, &Session::event, this, [this](const QodeAssist::ResponseEvent &ev) {
        onSessionEvent(m_session, ev);
    });
    connect(m_session, &Session::finished, this, [this](const LLMQore::RequestID &id, const QString &) {
        onSessionFinished(id);
    });
    connect(
        m_session,
        &Session::failed,
        this,
        [this](const LLMQore::RequestID &id, const QodeAssist::ErrorInfo &error) {
            onSessionFailed(id, error);
        });
    connect(m_session, &Session::cancelled, this, [this](const LLMQore::RequestID &id) {
        onSessionCancelled(id);
    });
}

bool ClientInterface::ensureAgentBound()
{
    if (m_session->hasAgent() && m_boundAgent == m_activeAgent)
        return true;

    QString agentError;
    if (!m_sessionManager->rebindAgentByName(m_session, m_activeAgent, &agentError)) {
        m_boundAgent.clear();
        const QString error = agentError.isEmpty() ? QStringLiteral("No chat agent selected")
                                                   : agentError;
        LOG_MESSAGE(error);
        emit errorOccurred(error);
        return false;
    }
    m_boundAgent = m_activeAgent;

    if (auto *client = m_session->client())
        m_sessionManager->toolContributors().contribute(client->tools());

    return true;
}

void ClientInterface::sendMessage(
    const QString &message, const QList<QString> &attachments, const QList<QString> &linkedFiles)
{
    if (message.trimmed().isEmpty() && attachments.isEmpty()) {
        LOG_MESSAGE("Ignoring empty chat message");
        return;
    }

    cancelRequest();

    Context::ChangesManager::instance().archiveAllNonArchivedEdits();

    QStringList imageFiles;
    QStringList textFiles;
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

    ensureSession();
    if (!m_session) {
        const QString error = QStringLiteral("Failed to create chat session");
        LOG_MESSAGE(error);
        emit errorOccurred(error);
        return;
    }

    if (!ensureAgentBound())
        return;

    auto *client = m_session->client();
    if (!client) {
        const QString error = QStringLiteral("Chat agent has no live client");
        LOG_MESSAGE(error);
        emit errorOccurred(error);
        return;
    }

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    Templates::ContextRenderer::Bindings bindings;
    bindings.projectDir = project ? project->projectDirectory().toFSPathString() : QString();
    bindings.configDir = AgentFactory::userConfigDir();
    m_session->setContextBindings(bindings);

    const QString chatFilePath = m_chatFilePath;
    m_session->setContentLoader([chatFilePath, cache = m_contentCache](const QString &storedPath) {
        return ChatSerializer::loadContentFromStorage(chatFilePath, storedPath, cache.get());
    });

    client->toolLoop()->setMaxRounds(Settings::toolsSettings().maxToolContinuations());
    client->setTransferTimeout(
        static_cast<int>(Settings::generalSettings().requestTimeout() * 1000));

    const QString chatContext = buildChatContextLayer();
    if (chatContext.isEmpty())
        m_session->systemPrompt()->clearLayer(QStringLiteral("chat.context"));
    else
        m_session->systemPrompt()->setLayer(QStringLiteral("chat.context"), chatContext);

    if (linkedFiles.isEmpty()) {
        m_session->unpinContext(QStringLiteral("chat.files"));
    } else {
        m_session->pinContext(
            QStringLiteral("chat.files"),
            [contextManager = QPointer<Context::ContextManager>(m_contextManager),
             linkedFiles]() -> QString {
                if (!contextManager)
                    return {};
                const auto contentFiles = contextManager->getContentFiles(linkedFiles);
                if (contentFiles.isEmpty())
                    return {};
                QString out = QStringLiteral(
                    "Linked files (current content, refreshed every request):\n");
                for (const auto &file : contentFiles) {
                    out += QStringLiteral("\nFile: %1\nContent:\n%2\n")
                               .arg(file.filename, file.content);
                }
                return out;
            });
    }

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    blocks.push_back(std::make_unique<LLMQore::TextContent>(message));

    for (const QString &skillName : invokedSkillNames(message)) {
        const auto skill = m_skillsManager->findByName(skillName);
        if (skill && !skill->body.isEmpty())
            blocks.push_back(std::make_unique<SkillInvocationContent>(skill->name, skill->body));
    }

    for (const auto &attachment : storedAttachments) {
        blocks.push_back(
            std::make_unique<StoredAttachmentContent>(attachment.filename, attachment.content));
    }

    for (const auto &image : storedImages) {
        blocks.push_back(
            std::make_unique<StoredImageContent>(image.fileName, image.storedPath, image.mediaType));
    }

    if (!m_chatFilePath.isEmpty()) {
        if (auto *todoTool = qobject_cast<QodeAssist::Tools::TodoTool *>(
                client->tools()->tool("todo_tool"))) {
            todoTool->setCurrentSessionId(m_chatFilePath);
        }
        if (auto *historyTool = qobject_cast<QodeAssist::Tools::ReadOriginalHistoryTool *>(
                client->tools()->tool("read_original_history"))) {
            historyTool->setCurrentSessionId(m_chatFilePath);
        }
    }

    const LLMQore::RequestID requestId = m_session->send(std::move(blocks));
    if (requestId.isEmpty()) {
        const QString error = QStringLiteral("Failed to start chat request for agent '%1': %2")
                                  .arg(m_activeAgent, m_session->lastError().message);
        LOG_MESSAGE(error);
        emit errorOccurred(error);
        return;
    }

    m_activeRequests[requestId] = {QJsonObject{{"id", requestId}}, m_session};

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

    QString applyError;
    if (!Context::ChangesManager::instance().applyPendingEditsForRequest(requestId, &applyError)) {
        LOG_MESSAGE(QString("Some edits for request %1 were not auto-applied: %2")
                        .arg(requestId, applyError));
    }

    emit messageReceivedCompletely();

    m_activeRequests.erase(it);
}

void ClientInterface::onSessionFailed(const QString &requestId, const QodeAssist::ErrorInfo &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    LOG_MESSAGE(QString("Chat request %1 failed: %2").arg(requestId, error.message));
    emit errorOccurred(error.message);

    m_activeRequests.erase(it);
}

void ClientInterface::onSessionCancelled(const QString &requestId)
{
    m_activeRequests.remove(requestId);
    emit requestCancelled();
}

QStringList ClientInterface::invokedSkillNames(const QString &message) const
{
    QStringList names;
    if (!m_skillsManager || !Settings::skillsSettings().enableSkills())
        return names;

    static const QRegularExpression skillCommand(QStringLiteral("(?:^|\\s)/([a-z0-9][a-z0-9-]*)"));
    auto skillMatch = skillCommand.globalMatch(message);
    while (skillMatch.hasNext()) {
        const QString skillName = skillMatch.next().captured(1);
        if (!names.contains(skillName))
            names << skillName;
    }
    return names;
}

QString ClientInterface::buildChatContextLayer() const
{
    QString context = Context::EnvBlockFormatter::formatProject(
        Context::EnvBlockFormatter::currentProject());

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    if (m_skillsManager && Settings::skillsSettings().enableSkills()) {
        QStringList projectSkillDirs;
        if (project) {
            Settings::ProjectSettings projectSettings(project);
            projectSkillDirs = Settings::SkillsSettings::splitLines(
                projectSettings.projectSkillDirs());
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
    }

    return context;
}

void ClientInterface::clearMessages()
{
    if (m_session)
        m_session->cancel();
    m_activeRequests.clear();
    if (m_history)
        m_history->clear();
}

void ClientInterface::cancelRequest()
{
    m_activeRequests.clear();
    if (m_session)
        m_session->cancel();

    LOG_MESSAGE("All chat requests cancelled and state cleared");
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
    if (m_chatFilePath != filePath)
        m_contentCache->clear();
    m_chatFilePath = filePath;
    m_chatModel->setChatFilePath(filePath);
}

QString ClientInterface::chatFilePath() const
{
    return m_chatFilePath;
}

} // namespace QodeAssist::Chat
