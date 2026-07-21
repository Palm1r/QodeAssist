// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatController.hpp"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <utils/filepath.h>

#include "ChatFileStore.hpp"
#include "LlmChatBackend.hpp"
#include "TurnContextAdapters.hpp"
#include "context/ChangesManager.h"
#include "logger/Logger.hpp"
#include "session/FileEditPayload.hpp"
#include "session/TurnContextBuilder.hpp"
#include "acp/AcpChatBackend.hpp"
#include "mcp/AgentKnowledgeServer.hpp"
#include "settings/ChatAssistantSettings.hpp"

namespace QodeAssist::Chat {

ChatController::ChatController(
    ChatModel *chatModel, Templates::IPromptProvider *promptProvider, QObject *parent)
    : QObject(parent)
    , m_promptProvider(promptProvider)
    , m_chatModel(chatModel)
    , m_contextManager(new Context::ContextManager(this))
    , m_session(new Session::Session(this))
    , m_llmBackend(new LlmChatBackend(promptProvider, this))
    , m_acpBackend(new Acp::AcpChatBackend(this))
    , m_agentKnowledge(new Mcp::AgentKnowledgeServer(this))
    , m_backend(m_llmBackend)
{
    connect(m_session, &Session::Session::rowsReset, m_chatModel, &ChatModel::resetMessages);
    connect(m_session, &Session::Session::rowsAppended, m_chatModel, &ChatModel::appendMessages);
    connect(m_session, &Session::Session::rowUpdated, m_chatModel, &ChatModel::updateMessage);
    connect(m_session, &Session::Session::rowsRemoved, m_chatModel, &ChatModel::removeMessages);
    m_chatModel->resetMessages(m_session->rows());

    m_acpBackend->setStoredContentLoader(&ChatFileStore::loadRawContentFromStorage);
    m_agentKnowledge->setIgnorePredicate([this](const QString &filePath) {
        auto *project = ProjectExplorer::ProjectManager::projectForFile(
            Utils::FilePath::fromString(filePath));
        return m_contextManager->ignoreManager()->shouldIgnore(filePath, project);
    });
    m_acpBackend->setKnowledgeService(m_agentKnowledge);

    connect(
        m_session,
        &Session::Session::sessionInfoReceived,
        this,
        &ChatController::sessionInfoReceived);

    connect(
        m_acpBackend,
        &Acp::AcpChatBackend::agentSessionUnavailable,
        this,
        &ChatController::agentSessionUnavailable);

    connect(
        m_acpBackend,
        &Acp::AcpChatBackend::availableCommandsChanged,
        this,
        &ChatController::agentCommandsChanged);

    m_session->setBackend(m_backend);

    connect(m_session, &Session::Session::turnStarted, this, &ChatController::requestStarted);
    connect(m_session, &Session::Session::turnFailed, this, &ChatController::errorOccurred);

    connect(m_session, &Session::Session::turnFinished, this, [this](const QString &turnId) {
        QString applyError;
        if (!Context::ChangesManager::instance().applyPendingEditsForRequest(turnId, &applyError)) {
            LOG_MESSAGE(QString("Some edits for request %1 were not auto-applied: %2")
                            .arg(turnId, applyError));
        }
        emit messageReceivedCompletely();
    });

    connect(m_session, &Session::Session::usageReceived, this, [this](const Session::Usage &usage) {
        emit messageUsageReceived(
            usage.promptTokens,
            usage.completionTokens,
            usage.cachedPromptTokens,
            usage.reasoningTokens);
    });

    connect(m_session, &Session::Session::rowsReset, this, [this] { registerHistoricalEdits(); });

    connect(
        m_session,
        &Session::Session::agentFileEditRecorded,
        this,
        [](const QString &turnId,
           const QString &editId,
           const QString &filePath,
           const QString &oldContent,
           const QString &newContent) {
            const QFileInfo info(filePath);
            const QString canonical = info.canonicalFilePath();
            const Utils::FilePath target = Utils::FilePath::fromString(
                canonical.isEmpty() ? info.absoluteFilePath() : canonical);

            bool insideProject = false;
            const QList<ProjectExplorer::Project *> projects
                = ProjectExplorer::ProjectManager::projects();
            for (const ProjectExplorer::Project *project : projects) {
                if (target.isChildOf(project->projectDirectory())) {
                    insideProject = true;
                    break;
                }
            }

            if (!insideProject) {
                LOG_MESSAGE(
                    QString("Agent edit %1 targets %2 outside every open project; recorded "
                            "in the transcript only, without apply/undo actions")
                        .arg(editId, filePath));
                return;
            }

            Context::ChangesManager::instance().registerAppliedFileEdit(
                editId, target.toUrlishString(), oldContent, newContent, turnId);
        });

    auto &changes = Context::ChangesManager::instance();
    connect(&changes, &Context::ChangesManager::fileEditApplied, this, [this](const QString &id) {
        m_session->updateFileEditStatus(id, "applied", "Successfully applied");
    });
    connect(&changes, &Context::ChangesManager::fileEditRejected, this, [this](const QString &id) {
        m_session->updateFileEditStatus(id, "rejected", "Rejected by user");
    });
    connect(&changes, &Context::ChangesManager::fileEditUndone, this, [this](const QString &id) {
        recordFileEditStatus(id, "rejected", "Successfully undone");
    });
    connect(&changes, &Context::ChangesManager::fileEditArchived, this, [this](const QString &id) {
        m_session->updateFileEditStatus(id, "archived", "Archived (from previous conversation turn)");
    });
}

void ChatController::setSkillsManager(Skills::SkillsManager *skillsManager)
{
    m_skillsManager = skillsManager;
}

void ChatController::bindAgent(const Acp::AgentDefinition &agent)
{
    m_acpBackend->bindAgent(agent);
    activateBackend(m_acpBackend);
}

void ChatController::bindLlm()
{
    activateBackend(m_llmBackend);
}

QString ChatController::boundAgentId() const
{
    return m_backend == m_acpBackend ? m_acpBackend->boundAgentId() : QString();
}

QString ChatController::boundAgentName() const
{
    return m_backend == m_acpBackend ? m_acpBackend->boundAgentName() : QString();
}

QList<LLMQore::Acp::AvailableCommand> ChatController::agentCommands() const
{
    if (m_backend != m_acpBackend)
        return {};
    return m_acpBackend->availableCommands();
}

bool ChatController::transcriptEmpty() const
{
    return m_session->rows().isEmpty();
}

Acp::AgentBinding ChatController::agentBinding() const
{
    if (m_backend != m_acpBackend)
        return {};

    return Acp::AgentBinding{m_acpBackend->boundAgentId(), m_acpBackend->bindingSessionId()};
}

void ChatController::resumeAgentSession(const QString &sessionId)
{
    m_acpBackend->resumeSession(sessionId);
}

void ChatController::startFreshAgentSession()
{
    m_acpBackend->startFreshSession();
}

void ChatController::startFreshAgentSession(const QString &handoverSummary)
{
    m_acpBackend->clearToolSession(m_chatFilePath);
    m_acpBackend->startFreshSession();
    m_acpBackend->setHandoverSummary(handoverSummary);
}

void ChatController::releaseAgentSession()
{
    m_acpBackend->clearToolSession(m_chatFilePath);
}

bool ChatController::conversationStarted() const
{
    return !m_session->history().messages().isEmpty();
}

void ChatController::activateBackend(Session::ChatBackend *backend)
{
    if (m_backend == backend)
        return;

    m_session->cancel();

    if (m_backend)
        m_backend->clearToolSession(m_chatFilePath);

    m_backend = backend;
    m_session->setBackend(backend);
    backend->setChatFilePath(m_chatFilePath);
}

Session::Session *ChatController::session() const
{
    return m_session;
}

void ChatController::sendMessage(const QString &message, const QList<QString> &attachments)
{
    if (message.trimmed().isEmpty() && attachments.isEmpty()) {
        LOG_MESSAGE("Ignoring empty chat message");
        return;
    }

    Context::ChangesManager::instance().archiveAllNonArchivedEdits();

    m_session->sendTurn(composeUserBlocks(message, attachments), buildTurnContext(message));
}

void ChatController::clearConversation()
{
    m_backend->clearToolSession(m_chatFilePath);
    m_session->clear();
}

void ChatController::cancelRequest()
{
    m_session->cancel();
}

void ChatController::resetToRow(int rowIndex)
{
    m_session->truncateRows(rowIndex);
}

void ChatController::respondToPermission(const QString &requestId, const QString &optionId)
{
    m_session->respondPermission(requestId, optionId);
}

QList<Session::ContentBlock> ChatController::composeUserBlocks(
    const QString &message, const QList<QString> &attachments)
{
    QList<QString> imageFiles;
    QList<QString> textFiles;

    for (const QString &filePath : attachments) {
        if (isImageFile(filePath))
            imageFiles.append(filePath);
        else
            textFiles.append(filePath);
    }

    QList<Session::ContentBlock> blocks{Session::TextBlock{message}};

    if (!textFiles.isEmpty() && !m_chatFilePath.isEmpty()) {
        for (const auto &file : m_contextManager->getContentFiles(textFiles)) {
            QString storedPath;
            if (!ChatFileStore::saveContentToStorage(
                    m_chatFilePath, file.filename, file.content.toUtf8().toBase64(), storedPath)) {
                continue;
            }
            blocks.append(Session::AttachmentBlock{file.filename, storedPath});
            LOG_MESSAGE(QString("Stored text file %1 as %2").arg(file.filename, storedPath));
        }
    } else if (!textFiles.isEmpty()) {
        LOG_MESSAGE(QString("Warning: Chat file path not set, cannot save %1 text file(s)")
                        .arg(textFiles.size()));
    }

    if (!imageFiles.isEmpty() && !m_chatFilePath.isEmpty()) {
        for (const QString &imagePath : imageFiles) {
            const QString base64Data = encodeImageToBase64(imagePath);
            if (base64Data.isEmpty())
                continue;

            const QFileInfo fileInfo(imagePath);
            QString storedPath;
            if (!ChatFileStore::saveContentToStorage(
                    m_chatFilePath, fileInfo.fileName(), base64Data, storedPath)) {
                continue;
            }

            blocks.append(
                Session::ImageBlock{fileInfo.fileName(), storedPath, getMediaTypeForImage(imagePath)});
            LOG_MESSAGE(QString("Stored image %1 as %2").arg(fileInfo.fileName(), storedPath));
        }
    } else if (!imageFiles.isEmpty()) {
        LOG_MESSAGE(QString("Warning: Chat file path not set, cannot save %1 image(s)")
                        .arg(imageFiles.size()));
    }

    return blocks;
}

Session::TurnContext ChatController::buildTurnContext(const QString &message) const
{
    auto &chatAssistantSettings = Settings::chatAssistantSettings();

    Session::TurnContextRequest contextRequest;
    contextRequest.message = message;
    contextRequest.needs = m_backend->contextNeeds();

    if (contextRequest.needs.systemPrompt)
        contextRequest.basePrompt = chatAssistantSettings.systemPrompt();

    auto *project = activeProject();

    ProjectContextQtCreator projectPort(project);
    auto skillsPort = makeSkillsContext(m_skillsManager, project);

    const Session::TurnContextBuilder builder(projectPort, skillsPort.get());

    return builder.build(contextRequest);
}

void ChatController::recordFileEditStatus(
    const QString &editId, const QString &status, const QString &fallbackMessage)
{
    const auto edit = Context::ChangesManager::instance().getFileEdit(editId);
    const QString message = edit.statusMessage.isEmpty() ? fallbackMessage : edit.statusMessage;
    m_session->updateFileEditStatus(editId, status, message);
}

void ChatController::registerHistoricalEdits()
{
    const QList<Session::MessageRow> rows = m_session->rows();

    for (const Session::MessageRow &row : rows) {
        if (row.kind != Session::RowKind::FileEdit)
            continue;

        const auto payload = Session::parseFileEditPayload(row.content);
        if (!payload) {
            LOG_MESSAGE(QString("Skipping unreadable file edit payload in row %1").arg(row.id));
            continue;
        }

        const QString editId = payload->value("edit_id").toString();
        const QString filePath = payload->value("file").toString();
        if (editId.isEmpty() || filePath.isEmpty())
            continue;

        if (!payload->value("old_content").isString() || !payload->value("new_content").isString()) {
            LOG_MESSAGE(
                QString("Skipping file edit %1: content fields are not strings").arg(editId));
            continue;
        }

        Context::ChangesManager::instance().addFileEdit(
            editId,
            filePath,
            payload->value("old_content").toString(),
            payload->value("new_content").toString(),
            false,
            true);

        m_session->updateFileEditStatus(editId, "archived", "Loaded from chat history");

        LOG_MESSAGE(QString(
                        "Registered historical file edit: %1 (original status: %2, now: "
                        "archived)")
                        .arg(editId, payload->value("status").toString()));
    }
}

Context::ContextManager *ChatController::contextManager() const
{
    return m_contextManager;
}

bool ChatController::isImageFile(const QString &filePath) const
{
    static const QSet<QString> imageExtensions = {"png", "jpg", "jpeg", "gif", "webp", "bmp", "svg"};

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    return imageExtensions.contains(extension);
}

QString ChatController::getMediaTypeForImage(const QString &filePath) const
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

QString ChatController::encodeImageToBase64(const QString &filePath) const
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

void ChatController::setChatFilePath(const QString &filePath)
{
    if (!m_chatFilePath.isEmpty() && m_chatFilePath != filePath)
        m_backend->clearToolSession(m_chatFilePath);

    m_chatFilePath = filePath;
    m_backend->setChatFilePath(filePath);
    m_chatModel->setChatFilePath(filePath);
}

QString ChatController::chatFilePath() const
{
    return m_chatFilePath;
}

} // namespace QodeAssist::Chat
