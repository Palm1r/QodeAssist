// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatRootView.hpp"

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMessageBox>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTextStream>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectmanager.h>
#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

#include "plugin/QodeAssistConstants.hpp"

#include "AgentRoleController.hpp"
#include "ChatAssistantSettings.hpp"
#include "ChatConfigurationController.hpp"
#include "ChatCompressor.hpp"
#include "ChatHistoryStore.hpp"
#include "FileEditController.hpp"
#include "GeneralSettings.hpp"
#include "InputTokenCounter.hpp"
#include "SettingsConstants.hpp"
#include "Logger.hpp"
#include "providers/ProvidersManager.hpp"
#include "SessionFileRegistry.hpp"
#include "context/ContextManager.hpp"
#include "context/RulesLoader.hpp"
#include "ProjectSettings.hpp"
#include "SkillsSettings.hpp"
#include "skills/SkillsManager.hpp"

namespace QodeAssist::Chat {

namespace {
bool isChatEditor(Core::IEditor *editor)
{
    return editor && editor->document()
           && editor->document()->id() == Utils::Id(Constants::QODE_ASSIST_CHAT_EDITOR_ID);
}

QKeySequence sendMessageKeySequence()
{
    auto command = Core::ActionManager::command(Constants::QODE_ASSIST_CHAT_SEND_MESSAGE);
    if (!command)
        return {};

    QKeySequence sequence = command->keySequence();
    if (sequence.isEmpty()) {
        const QList<QKeySequence> defaults = command->defaultKeySequences();
        if (!defaults.isEmpty())
            sequence = defaults.constFirst();
    }
    return sequence;
}
} // namespace

ChatRootView::ChatRootView(QQuickItem *parent)
    : QQuickItem(parent)
    , m_chatModel(new ChatModel(this))
    , m_promptProvider(Templates::PromptTemplateManager::instance())
    , m_controller(new ChatController(m_chatModel, &m_promptProvider, this))
    , m_fileManager(new ChatFileManager(this))
    , m_isRequestInProgress(false)
    , m_chatCompressor(new ChatCompressor(this))
    , m_agentRoleController(new AgentRoleController(this))
    , m_configurationController(new ChatConfigurationController(this))
    , m_fileEditController(new FileEditController(this))
    , m_tokenCounter(new InputTokenCounter(
          m_controller->session(), m_controller->contextManager(), this))
    , m_historyStore(new ChatHistoryStore(m_controller->session(), this))
{
    m_isSyncOpenFiles = Settings::chatAssistantSettings().linkOpenFiles();
    connect(
        &Settings::chatAssistantSettings().linkOpenFiles,
        &Utils::BaseAspect::changed,
        this,
        [this]() { setIsSyncOpenFiles(Settings::chatAssistantSettings().linkOpenFiles()); });

    QMetaObject::invokeMethod(
        this,
        [this] {
            if (auto sendCommand
                = Core::ActionManager::command(Constants::QODE_ASSIST_CHAT_SEND_MESSAGE)) {
                connect(
                    sendCommand,
                    &Core::Command::keySequenceChanged,
                    this,
                    &ChatRootView::sendShortcutTextChanged,
                    Qt::UniqueConnection);
            }
            emit sendShortcutTextChanged();
        },
        Qt::QueuedConnection);

    auto &settings = Settings::generalSettings();

    connect(
        &settings.caModel, &Utils::BaseAspect::changed, this, &ChatRootView::currentTemplateChanged);

    connect(
        m_configurationController,
        &ChatConfigurationController::availableConfigurationsChanged,
        this,
        &ChatRootView::availableConfigurationsChanged);
    connect(
        m_configurationController,
        &ChatConfigurationController::currentConfigurationChanged,
        this,
        &ChatRootView::currentConfigurationChanged);
    connect(
        m_configurationController,
        &ChatConfigurationController::agentRequested,
        this,
        &ChatRootView::handleAgentRequested);
    connect(
        m_configurationController,
        &ChatConfigurationController::llmRequested,
        this,
        &ChatRootView::handleLlmRequested);

    connect(
        m_controller,
        &ChatController::messageReceivedCompletely,
        this,
        &ChatRootView::autosave);

    connect(m_controller, &ChatController::messageReceivedCompletely, this, [this]() {
        this->setRequestProgressStatus(false);
    });

    connect(
        m_controller,
        &ChatController::messageReceivedCompletely,
        this,
        &ChatRootView::updateInputTokensCount);

    connect(m_chatModel, &ChatModel::modelReseted, this, [this]() {
        setRecentFilePath(QString{});
        m_agentSuggestedTitle.clear();
        setAgentSessionIssue(QString(), false);
        m_fileEditController->clearCurrentRequestId();
    });
    auto maybeEmitTitle = [this] {
        const QString newTitle = computeChatTitle();
        if (newTitle == m_cachedChatTitle)
            return;
        m_cachedChatTitle = newTitle;
        emit chatTitleChanged();
    };
    connect(
        m_controller,
        &ChatController::agentTitleSuggested,
        this,
        [this, maybeEmitTitle](const QString &title) {
            m_agentSuggestedTitle = title;
            maybeEmitTitle();
        });

    connect(
        m_controller,
        &ChatController::agentSessionUnavailable,
        this,
        [this](const QString &reason) {
            LOG_MESSAGE(QString("Agent session could not be reopened: %1").arg(reason));
            setAgentSessionIssue(
                tr("The previous session with this agent could not be reopened, so the transcript "
                   "is read-only. Start a new session to keep working with this agent — it will "
                   "not have the context above."),
                true);
        });

    m_historyStore->setBindingReader([this] {
        return m_quarantinedBinding.isEmpty() ? m_controller->agentBinding() : m_quarantinedBinding;
    });
    m_historyStore->setBindingWriter(
        [this](const Acp::AgentBinding &binding) { restoreAgentBinding(binding); });
    connect(m_chatModel, &ChatModel::modelReseted, this, maybeEmitTitle);
    connect(m_chatModel, &QAbstractItemModel::modelReset, this, maybeEmitTitle);
    connect(m_chatModel, &QAbstractItemModel::rowsInserted, this, maybeEmitTitle);
    connect(m_chatModel, &QAbstractItemModel::rowsRemoved, this, maybeEmitTitle);
    connect(this, &ChatRootView::attachmentFilesChanged, this, [this]() {
        m_tokenCounter->setAttachments(m_attachmentFiles);
    });
    connect(this, &ChatRootView::linkedFilesChanged, this, [this]() {
        m_tokenCounter->setLinkedFiles(m_linkedFiles);
    });
    connect(this, &ChatRootView::useToolsChanged, this, &ChatRootView::updateInputTokensCount);

    connect(
        m_tokenCounter,
        &InputTokenCounter::inputTokensChanged,
        this,
        &ChatRootView::inputTokensCountChanged);
    connect(
        m_agentRoleController,
        &AgentRoleController::availableRolesChanged,
        this,
        &ChatRootView::availableAgentRolesChanged);
    connect(
        m_agentRoleController,
        &AgentRoleController::currentRoleChanged,
        this,
        &ChatRootView::currentAgentRoleChanged);
    connect(
        m_agentRoleController,
        &AgentRoleController::baseSystemPromptChanged,
        this,
        &ChatRootView::baseSystemPromptChanged);

    auto editors = Core::EditorManager::instance();

    connect(editors, &Core::EditorManager::editorCreated, this, &ChatRootView::onEditorCreated);
    connect(
        editors,
        &Core::EditorManager::editorAboutToClose,
        this,
        &ChatRootView::onEditorAboutToClose);

    connect(editors, &Core::EditorManager::currentEditorAboutToChange, this, [this]() {
        if (m_isSyncOpenFiles) {
            for (auto editor : std::as_const(m_currentEditors)) {
                onAppendLinkFileFromEditor(editor);
            }
        }
    });
    connect(
        &Settings::chatAssistantSettings().textFontFamily,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::textFamilyChanged);
    connect(
        &Settings::chatAssistantSettings().codeFontFamily,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::codeFamilyChanged);
    connect(
        &Settings::chatAssistantSettings().textFontSize,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::textFontSizeChanged);
    connect(
        &Settings::chatAssistantSettings().codeFontSize,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::codeFontSizeChanged);
    connect(
        &Settings::chatAssistantSettings().textFormat,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::textFormatChanged);
    connect(m_controller, &ChatController::errorOccurred, this, [this](const QString &error) {
        this->setRequestProgressStatus(false);
        m_lastErrorMessage = error;
        emit lastErrorMessageChanged();
    });

    connect(
        m_controller,
        &ChatController::requestStarted,
        this,
        [this](const QString &requestId) { m_fileEditController->setCurrentRequestId(requestId); });

    connect(
        m_controller,
        &ChatController::messageUsageReceived,
        this,
        [this](int promptTokens, int /*completionTokens*/, int /*cached*/, int /*reasoning*/) {
            m_tokenCounter->recordServerUsage(promptTokens);
        });

    connect(
        m_fileEditController,
        &FileEditController::statsChanged,
        this,
        &ChatRootView::currentMessageEditsStatsChanged);
    connect(m_fileEditController, &FileEditController::infoMessage, this, [this](const QString &m) {
        m_lastInfoMessage = m;
        emit lastInfoMessageChanged();
    });
    connect(m_fileEditController, &FileEditController::errorOccurred, this, [this](const QString &e) {
        m_lastErrorMessage = e;
        emit lastErrorMessageChanged();
    });

    connect(
        m_historyStore, &ChatHistoryStore::saveRequested, this, &ChatRootView::saveHistory);
    connect(
        m_historyStore, &ChatHistoryStore::loadRequested, this, &ChatRootView::loadHistory);

    refreshRules();

    connect(
        ProjectExplorer::ProjectManager::instance(),
        &ProjectExplorer::ProjectManager::startupProjectChanged,
        this,
        &ChatRootView::refreshRules);

    connect(
        ProjectExplorer::ProjectManager::instance(),
        &ProjectExplorer::ProjectManager::projectAdded,
        this,
        &ChatRootView::openFilesChanged);

    connect(
        ProjectExplorer::ProjectManager::instance(),
        &ProjectExplorer::ProjectManager::projectRemoved,
        this,
        &ChatRootView::openFilesChanged);

    connect(
        &Settings::chatAssistantSettings().enableChatTools,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::useToolsChanged);

    connect(
        &Settings::chatAssistantSettings().enableThinkingMode,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::useThinkingChanged);

    connect(
        &Settings::generalSettings().caProvider,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::isThinkingSupportChanged);

    connect(m_fileManager, &ChatFileManager::fileOperationFailed, this, [this](const QString &error) {
        m_lastErrorMessage = error;
        emit lastErrorMessageChanged();
    });

    connect(m_chatCompressor, &ChatCompressor::compressionStarted, this, [this]() {
        emit isCompressingChanged();
    });

    connect(m_chatCompressor, &ChatCompressor::compressionCompleted, this, [this](const QString &compressedChatPath) {
        emit isCompressingChanged();
        m_lastInfoMessage = tr("Chat compressed successfully!");
        emit lastInfoMessageChanged();
        emit compressionCompleted(compressedChatPath);

        loadHistory(compressedChatPath);

        if (m_pendingSend.active) {
            PendingSend p = m_pendingSend;
            m_pendingSend = {};
            dispatchSend(p.message, p.attachments, p.linkedFiles, p.useTools, p.useThinking);
        }
    });

    connect(
        m_chatCompressor,
        &ChatCompressor::compressingChanged,
        this,
        &ChatRootView::isCompressingChanged);

    connect(m_chatCompressor, &ChatCompressor::summaryReady, this, [this](const QString &summary) {
        m_controller->startFreshAgentSession(summary);
        setAgentSessionIssue(QString(), false);
    });

    connect(m_chatCompressor, &ChatCompressor::compressionFailed, this, [this](const QString &error) {
        emit isCompressingChanged();
        m_lastErrorMessage = error;
        emit lastErrorMessageChanged();
        emit compressionFailed(error);

        if (m_pendingSend.active) {
            PendingSend p = m_pendingSend;
            m_pendingSend = {};
            dispatchSend(p.message, p.attachments, p.linkedFiles, p.useTools, p.useThinking);
        }
    });
}

ChatRootView::~ChatRootView()
{
    if (m_sessionFileRegistry && !m_recentFilePath.isEmpty()) {
        m_sessionFileRegistry->release(m_recentFilePath);
    }
}

SessionFileRegistry *ChatRootView::sessionFileRegistry() const
{
    if (!m_sessionFileRegistryResolved) {
        m_sessionFileRegistryResolved = true;
        if (auto context = qmlContext(this)) {
            m_sessionFileRegistry = qobject_cast<SessionFileRegistry *>(
                context->contextProperty("sessionFileRegistry").value<QObject *>());
        }
    }
    return m_sessionFileRegistry;
}

Skills::SkillsManager *ChatRootView::skillsManager() const
{
    if (!m_skillsManagerResolved) {
        m_skillsManagerResolved = true;
        if (auto context = qmlContext(this)) {
            m_skillsManager = qobject_cast<Skills::SkillsManager *>(
                context->contextProperty("skillsManager").value<QObject *>());
        }
    }
    return m_skillsManager;
}

QVariantList ChatRootView::searchSkills(const QString &query) const
{
    QVariantList results;
    auto *manager = skillsManager();
    if (!manager || !Settings::skillsSettings().enableSkills())
        return results;

    auto *project = Context::RulesLoader::getActiveProject();
    QStringList projectSkillDirs;
    if (project) {
        Settings::ProjectSettings projectSettings(project);
        projectSkillDirs = Settings::SkillsSettings::splitLines(
            projectSettings.projectSkillDirs());
    }
    manager->configure(
        project ? project->projectDirectory().toFSPathString() : QString(),
        Settings::SkillsSettings::splitPaths(Settings::skillsSettings().globalSkillRoots()),
        projectSkillDirs);

    const QString needle = query.trimmed().toLower();
    for (const Skills::AgentSkill &skill : manager->skills()) {
        if (!skill.enabled)
            continue;
        if (!needle.isEmpty() && !skill.name.toLower().contains(needle)
            && !skill.description.toLower().contains(needle)) {
            continue;
        }
        results.append(QVariantMap{
            {QStringLiteral("name"), skill.name},
            {QStringLiteral("description"), skill.description}});
    }
    return results;
}

ChatModel *ChatRootView::chatModel() const
{
    return m_chatModel;
}

bool ChatRootView::refuseWhileReadOnly()
{
    if (m_agentSessionIssue.isEmpty())
        return false;

    m_lastErrorMessage = m_agentSessionIssue;
    emit lastErrorMessageChanged();
    return true;
}

void ChatRootView::sendMessage(const QString &message)
{
    if (refuseWhileReadOnly())
        return;

    const QStringList attachments = m_attachmentFiles;
    const QStringList linkedFiles = m_linkedFiles;
    const bool tools = useTools();
    const bool thinking = useThinking();

    if (deferSendForAutoCompress(message, attachments, linkedFiles, tools, thinking))
        return;

    dispatchSend(message, attachments, linkedFiles, tools, thinking);
}

bool ChatRootView::deferSendForAutoCompress(
    const QString &message,
    const QStringList &attachments,
    const QStringList &linkedFiles,
    bool useToolsArg,
    bool useThinkingArg)
{
    if (isAgentBound())
        return false;

    auto &settings = Settings::chatAssistantSettings();
    if (!settings.autoCompress())
        return false;

    const int threshold = settings.autoCompressThreshold();
    const int inputTokens = m_tokenCounter->inputTokens();
    if (inputTokens < threshold)
        return false;

    if (m_recentFilePath.isEmpty()) {
        QString filePath = getAutosaveFilePath(message, attachments);
        if (filePath.isEmpty())
            return false;
        setRecentFilePath(filePath);
        LOG_MESSAGE(QString("Set chat file path for new chat (auto-compress): %1").arg(filePath));
    }

    if (m_chatCompressor->isCompressing() || m_pendingSend.active)
        return false;

    LOG_MESSAGE(QString("Auto-compress preempt: estimated next=%1 ≥ threshold=%2; deferring send")
                    .arg(inputTokens)
                    .arg(threshold));

    m_pendingSend = {message, attachments, linkedFiles, useToolsArg, useThinkingArg, true};
    compressCurrentChat();
    return true;
}

void ChatRootView::dispatchSend(
    const QString &message,
    const QStringList &attachments,
    const QStringList &linkedFiles,
    bool useToolsArg,
    bool useThinkingArg)
{
    if (refuseWhileReadOnly())
        return;

    if (m_recentFilePath.isEmpty()) {
        QString filePath = getAutosaveFilePath(message, attachments);
        if (auto registry = sessionFileRegistry()) {
            filePath = registry->uniqueFreePath(filePath);
        }
        if (!filePath.isEmpty()) {
            setRecentFilePath(filePath);
            LOG_MESSAGE(QString("Set chat file path for new chat: %1").arg(filePath));
        }
    }

    m_tokenCounter->recordSent();
    setRequestProgressStatus(true);

    m_controller->setSkillsManager(skillsManager());
    m_controller->sendMessage(message, attachments, linkedFiles, useToolsArg, useThinkingArg);

    m_fileManager->clearIntermediateStorage();
    clearAttachmentFiles();
}

void ChatRootView::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void ChatRootView::cancelRequest()
{
    m_controller->cancelRequest();
    setRequestProgressStatus(false);
}

void ChatRootView::clearAttachmentFiles()
{
    if (m_attachmentFiles.isEmpty()) {
        return;
    }

    m_attachmentFiles.clear();
    emit attachmentFilesChanged();
    m_fileManager->clearIntermediateStorage();
}

void ChatRootView::clearLinkedFiles()
{
    if (m_linkedFiles.isEmpty()) {
        return;
    }

    m_linkedFiles.clear();
    emit linkedFilesChanged();
}

void ChatRootView::clearMessages()
{
    m_controller->clearMessages();
    clearLinkedFiles();
}

void ChatRootView::resetChatToMessage(int index)
{
    if (refuseWhileReadOnly())
        return;

    m_controller->resetToRow(index);
    setRequestProgressStatus(false);
}

QString ChatRootView::currentTemplate() const
{
    auto &settings = Settings::generalSettings();
    return settings.caModel();
}

void ChatRootView::saveHistory(const QString &filePath)
{
    if (filePath != m_recentFilePath) {
        if (auto registry = sessionFileRegistry(); registry && registry->isLocked(filePath)) {
            m_lastErrorMessage
                = tr("This chat file is already in use by another QodeAssist chat session.");
            emit lastErrorMessageChanged();
            return;
        }
    }

    auto result = m_historyStore->save(filePath);
    if (!result.success) {
        LOG_MESSAGE(QString("Failed to save chat history: %1").arg(result.errorMessage));
    } else {
        setRecentFilePath(filePath);
    }
}

void ChatRootView::loadHistory(const QString &filePath)
{
    if (filePath != m_recentFilePath) {
        if (auto registry = sessionFileRegistry(); registry && registry->isLocked(filePath)) {
            m_lastErrorMessage
                = tr("This chat is already open in another QodeAssist chat session.");
            emit lastErrorMessageChanged();
            return;
        }
    }

    auto result = m_historyStore->load(filePath);
    if (!result.success) {
        LOG_MESSAGE(QString("Failed to load chat history: %1").arg(result.errorMessage));
    } else {
        setRecentFilePath(filePath);
        setRequestProgressStatus(false);
        if (!result.warningMessage.isEmpty()) {
            m_lastErrorMessage = result.warningMessage;
            emit lastErrorMessageChanged();
        }
    }

    if (!m_pendingSend.active)
        m_fileManager->clearIntermediateStorage();
    m_attachmentFiles.clear();
    m_linkedFiles.clear();
    emit attachmentFilesChanged();
    emit linkedFilesChanged();

    m_fileEditController->clearCurrentRequestId();
    updateInputTokensCount();
}

void ChatRootView::showSaveDialog()
{
    m_historyStore->showSaveDialog();
}

void ChatRootView::showLoadDialog()
{
    m_historyStore->showLoadDialog();
}

void ChatRootView::autosave()
{
    if (m_chatModel->rowCount() == 0 || !Settings::chatAssistantSettings().autosave()) {
        return;
    }

    if (m_recentFilePath.isEmpty()) {
        QString filePath = getAutosaveFilePath();
        if (auto registry = sessionFileRegistry()) {
            filePath = registry->uniqueFreePath(filePath);
        }
        if (filePath.isEmpty()) {
            return;
        }
        setRecentFilePath(filePath);
    }

    m_historyStore->save(m_recentFilePath);
}

QString ChatRootView::getAutosaveFilePath() const
{
    return m_historyStore->autosaveFilePath(m_recentFilePath);
}

QString ChatRootView::getAutosaveFilePath(
    const QString &firstMessage, const QStringList &attachments) const
{
    return m_historyStore
        ->autosaveFilePath(m_recentFilePath, firstMessage, hasImageAttachments(attachments));
}

QStringList ChatRootView::attachmentFiles() const
{
    return m_attachmentFiles;
}

QStringList ChatRootView::linkedFiles() const
{
    return m_linkedFiles;
}

void ChatRootView::showAttachFilesDialog()
{
    QFileDialog dialog(nullptr, tr("Select Files to Attach"));
    dialog.setFileMode(QFileDialog::ExistingFiles);

    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        dialog.setDirectory(project->projectDirectory().toFSPathString());
    }

    if (dialog.exec() == QDialog::Accepted) {
        addFilesToAttachList(dialog.selectedFiles());
    }
}

void ChatRootView::addFilesToAttachList(const QStringList &filePaths)
{
    if (filePaths.isEmpty()) {
        return;
    }

    const QStringList processedPaths = m_fileManager->processDroppedFiles(filePaths);

    bool filesAdded = false;
    for (const QString &filePath : processedPaths) {
        if (!m_attachmentFiles.contains(filePath)) {
            m_attachmentFiles.append(filePath);
            filesAdded = true;
        }
    }

    if (filesAdded) {
        emit attachmentFilesChanged();
    }
}

void ChatRootView::removeFileFromAttachList(int index)
{
    if (index < 0 || index >= m_attachmentFiles.size()) {
        return;
    }

    const QString removedFile = m_attachmentFiles.at(index);
    m_attachmentFiles.removeAt(index);
    emit attachmentFilesChanged();

    LOG_MESSAGE(QString("Removed attachment file: %1").arg(removedFile));
}

void ChatRootView::showLinkFilesDialog()
{
    QFileDialog dialog(nullptr, tr("Select Files to Attach"));
    dialog.setFileMode(QFileDialog::ExistingFiles);

    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        dialog.setDirectory(project->projectDirectory().toFSPathString());
    }

    if (dialog.exec() == QDialog::Accepted) {
        addFilesToLinkList(dialog.selectedFiles());
    }
}

void ChatRootView::addFilesToLinkList(const QStringList &filePaths)
{
    if (filePaths.isEmpty()) {
        return;
    }

    bool filesAdded = false;
    QStringList imageFiles;

    for (const QString &filePath : filePaths) {
        if (isImageFile(filePath)) {
            imageFiles.append(filePath);
            continue;
        }

        if (!m_linkedFiles.contains(filePath)) {
            m_linkedFiles.append(filePath);
            filesAdded = true;
        }
    }

    if (!imageFiles.isEmpty()) {
        addFilesToAttachList(imageFiles);
        m_lastInfoMessage
            = tr("Images automatically moved to Attach zone (%n file(s))", "", imageFiles.size());
        emit lastInfoMessageChanged();
    }

    if (filesAdded) {
        emit linkedFilesChanged();
    }
}

void ChatRootView::removeFileFromLinkList(int index)
{
    if (index < 0 || index >= m_linkedFiles.size()) {
        return;
    }

    const QString removedFile = m_linkedFiles.at(index);
    m_linkedFiles.removeAt(index);
    emit linkedFilesChanged();

    LOG_MESSAGE(QString("Removed linked file: %1").arg(removedFile));
}

void ChatRootView::showAddImageDialog()
{
    QFileDialog dialog(nullptr, tr("Select Images to Attach"));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter(tr("Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp)"));

    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        dialog.setDirectory(project->projectDirectory().toFSPathString());
    }

    if (dialog.exec() == QDialog::Accepted) {
        addFilesToAttachList(dialog.selectedFiles());
    }
}

QStringList ChatRootView::convertUrlsToLocalPaths(const QVariantList &urls) const
{
    QStringList localPaths;
    for (const QVariant &urlVariant : urls) {
        QUrl url(urlVariant.toString());
        if (url.isLocalFile()) {
            QString localPath = url.toLocalFile();
            if (!localPath.isEmpty()) {
                localPaths.append(localPath);
            }
        }
    }
    return localPaths;
}

void ChatRootView::calculateMessageTokensCount(const QString &message)
{
    m_tokenCounter->setMessage(message);
}

bool ChatRootView::isSendShortcut(int key, int modifiers) const
{
    const QKeySequence sequence = sendMessageKeySequence();
    if (sequence.isEmpty())
        return false;

    const QKeyCombination combination = sequence[0];
    const int sequenceKey = combination.key();

    const int relevantMask = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier
                             | Qt::MetaModifier;
    const int sequenceModifiers = combination.keyboardModifiers() & relevantMask;
    const int eventModifiers = modifiers & relevantMask;

    const bool isReturnLike = sequenceKey == Qt::Key_Return || sequenceKey == Qt::Key_Enter;
    const bool keyMatches = key == sequenceKey
        || (isReturnLike && (key == Qt::Key_Return || key == Qt::Key_Enter));

    return keyMatches && eventModifiers == sequenceModifiers;
}

QString ChatRootView::sendShortcutText() const
{
    return sendMessageKeySequence().toString(QKeySequence::NativeText);
}

void ChatRootView::setIsSyncOpenFiles(bool state)
{
    if (m_isSyncOpenFiles != state) {
        m_isSyncOpenFiles = state;
        emit isSyncOpenFilesChanged();
    }

    if (m_isSyncOpenFiles) {
        for (auto editor : std::as_const(m_currentEditors)) {
            onAppendLinkFileFromEditor(editor);
        }
    }
}

void ChatRootView::openChatHistoryFolder()
{
    m_historyStore->openHistoryFolder();
}

void ChatRootView::openRulesFolder()
{
    auto project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        return;
    }

    QString projectPath = project->projectDirectory().toFSPathString();
    QString rulesPath = QDir(projectPath).filePath(".qodeassist/rules");

    QDir dir(rulesPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QUrl url = QUrl::fromLocalFile(dir.absolutePath());
    QDesktopServices::openUrl(url);
}

void ChatRootView::openSettings()
{
    QMetaObject::invokeMethod(
        this,
        []() { Settings::showSettings(Constants::QODE_ASSIST_CHAT_ASSISTANT_SETTINGS_PAGE_ID); },
        Qt::QueuedConnection);
}

void ChatRootView::openFileInEditor(const QString &filePath)
{
    if (filePath.isEmpty())
        return;
    Core::EditorManager::openEditor(Utils::FilePath::fromString(filePath));
}

void ChatRootView::triggerOpenChatCommand(Utils::Id commandId)
{
    if (auto command = Core::ActionManager::command(commandId)) {
        if (auto action = command->action())
            action->trigger();
    }
}

bool ChatRootView::isInEditor() const
{
    return m_isInEditor;
}

void ChatRootView::setInEditor(bool value)
{
    if (m_isInEditor == value)
        return;
    m_isInEditor = value;
    emit isInEditorChanged();
}

void ChatRootView::requestNewChat()
{
    triggerOpenChatCommand(Constants::QODE_ASSIST_NEW_CHAT_ACTION);
}

QString ChatRootView::chatTitle() const
{
    if (m_cachedChatTitle.isEmpty())
        m_cachedChatTitle = computeChatTitle();
    return m_cachedChatTitle;
}

QString ChatRootView::computeChatTitle() const
{
    if (!m_agentSuggestedTitle.isEmpty())
        return m_agentSuggestedTitle;

    for (const Session::MessageRow &row : m_controller->session()->rows()) {
        if (row.kind != Session::RowKind::User)
            continue;
        const QString content = row.content.trimmed();
        if (content.isEmpty())
            continue;
        const QString firstLine = content.section(QChar('\n'), 0, 0).trimmed();
        constexpr int maxLen = 60;
        if (firstLine.length() > maxLen)
            return firstLine.left(maxLen - 1) + QChar(0x2026);
        return firstLine;
    }
    return {};
}

void ChatRootView::handOffSession()
{
    if (m_chatModel->rowCount() > 0) {
        if (m_recentFilePath.isEmpty()) {
            QString filePath = getAutosaveFilePath();
            if (auto registry = sessionFileRegistry())
                filePath = registry->uniqueFreePath(filePath);
            if (!filePath.isEmpty())
                setRecentFilePath(filePath);
        }
        if (!m_recentFilePath.isEmpty())
            m_historyStore->save(m_recentFilePath);
    }

    if (auto registry = sessionFileRegistry(); registry && !m_recentFilePath.isEmpty())
        registry->setPendingChatFile(m_recentFilePath);

    setRecentFilePath(QString{});
}

void ChatRootView::consumePendingChatFile()
{
    if (auto registry = sessionFileRegistry()) {
        const QString pending = registry->takePendingChatFile();
        if (!pending.isEmpty())
            loadHistory(pending);
    }
}

void ChatRootView::relocateToSplit()
{
    handOffSession();
    triggerOpenChatCommand(Constants::QODE_ASSIST_NEW_CHAT_ACTION);
    clearMessages();
    clearAttachmentFiles();
    emit closeHostRequested();
}

void ChatRootView::relocateToWindow()
{
    handOffSession();
    triggerOpenChatCommand(Constants::QODE_ASSIST_OPEN_CHAT_WINDOW_ACTION);
    clearMessages();
    clearAttachmentFiles();
    emit closeHostRequested();

    // Closing the source split raises the main window; re-raise the chat window once that
    // queued teardown has run. The registry outlives this view, which the split close deletes.
    if (auto registry = sessionFileRegistry()) {
        QMetaObject::invokeMethod(
            registry,
            [] {
                if (auto command = Core::ActionManager::command(
                        Constants::QODE_ASSIST_OPEN_CHAT_WINDOW_ACTION)) {
                    if (auto action = command->action())
                        action->trigger();
                }
            },
            Qt::QueuedConnection);
    }
}

void ChatRootView::updateInputTokensCount()
{
    m_tokenCounter->recompute();
}

int ChatRootView::inputTokensCount() const
{
    return m_tokenCounter->inputTokens();
}

bool ChatRootView::isSyncOpenFiles() const
{
    return m_isSyncOpenFiles;
}

void ChatRootView::onEditorAboutToClose(Core::IEditor *editor)
{
    if (isChatEditor(editor)) {
        return;
    }

    if (auto document = editor->document(); document && isSyncOpenFiles()) {
        QString filePath = document->filePath().toFSPathString();
        m_linkedFiles.removeOne(filePath);
        emit linkedFilesChanged();
    }

    if (editor) {
        m_currentEditors.removeOne(editor);
    }

    emit openFilesChanged();
}

void ChatRootView::onAppendLinkFileFromEditor(Core::IEditor *editor)
{
    if (isChatEditor(editor)) {
        return;
    }

    if (auto document = editor->document(); document && isSyncOpenFiles()) {
        QString filePath = document->filePath().toFSPathString();
        if (!m_linkedFiles.contains(filePath) && !shouldIgnoreFileForAttach(document->filePath())) {
            m_linkedFiles.append(filePath);
            emit linkedFilesChanged();
        }
    }
}

void ChatRootView::onEditorCreated(Core::IEditor *editor, const Utils::FilePath &filePath)
{
    if (isChatEditor(editor)) {
        return;
    }

    if (editor && editor->document()) {
        m_currentEditors.append(editor);
        emit openFilesChanged();
    }
}

QString ChatRootView::chatFileName() const
{
    return QFileInfo(m_recentFilePath).baseName();
}

QString ChatRootView::chatFilePath() const
{
    return m_recentFilePath;
}

void ChatRootView::setRecentFilePath(const QString &filePath)
{
    if (m_recentFilePath == filePath) {
        return;
    }

    if (auto registry = sessionFileRegistry()) {
        if (!m_recentFilePath.isEmpty()) {
            registry->release(m_recentFilePath);
        }
        if (!filePath.isEmpty()) {
            registry->lock(filePath);
        }
    }

    m_recentFilePath = filePath;
    m_controller->setChatFilePath(filePath);
    m_fileManager->setChatFilePath(filePath);
    emit chatFileNameChanged();
}

bool ChatRootView::shouldIgnoreFileForAttach(const Utils::FilePath &filePath)
{
    auto project = ProjectExplorer::ProjectManager::projectForFile(filePath);
    if (project
        && m_controller->contextManager()
               ->ignoreManager()
               ->shouldIgnore(filePath.toFSPathString(), project)) {
        LOG_MESSAGE(QString("Ignoring file for attachment due to .qodeassistignore: %1")
                        .arg(filePath.toFSPathString()));
        return true;
    }

    return false;
}

QString ChatRootView::textFontFamily() const
{
    return Settings::chatAssistantSettings().textFontFamily.stringValue();
}

QString ChatRootView::codeFontFamily() const
{
    return Settings::chatAssistantSettings().codeFontFamily.stringValue();
}

int ChatRootView::codeFontSize() const
{
    return Settings::chatAssistantSettings().codeFontSize();
}

int ChatRootView::textFontSize() const
{
    return Settings::chatAssistantSettings().textFontSize();
}

int ChatRootView::textFormat() const
{
    return Settings::chatAssistantSettings().textFormat();
}

bool ChatRootView::isRequestInProgress() const
{
    return m_isRequestInProgress;
}

void ChatRootView::setRequestProgressStatus(bool state)
{
    if (m_isRequestInProgress == state)
        return;
    m_isRequestInProgress = state;
    emit isRequestInProgressChanged();
}

QString ChatRootView::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

QVariantList ChatRootView::activeRules() const
{
    return m_activeRules;
}

int ChatRootView::activeRulesCount() const
{
    return m_activeRules.size();
}

QString ChatRootView::getRuleContent(int index)
{
    if (index < 0 || index >= m_activeRules.size())
        return QString();

    return Context::RulesLoader::loadRuleFileContent(
        m_activeRules[index].toMap()["filePath"].toString());
}

void ChatRootView::refreshRules()
{
    m_activeRules.clear();

    auto project = Context::RulesLoader::getActiveProject();
    if (!project) {
        emit activeRulesChanged();
        emit activeRulesCountChanged();
        return;
    }

    auto ruleFiles
        = Context::RulesLoader::getRuleFilesForProject(project, Context::RulesContext::Chat);

    for (const auto &ruleFile : ruleFiles) {
        QVariantMap ruleMap;
        ruleMap["filePath"] = ruleFile.filePath;
        ruleMap["fileName"] = ruleFile.fileName;
        ruleMap["category"] = ruleFile.category;
        m_activeRules.append(ruleMap);
    }

    emit activeRulesChanged();
    emit activeRulesCountChanged();
}

bool ChatRootView::useTools() const
{
    return Settings::chatAssistantSettings().enableChatTools();
}

void ChatRootView::setUseTools(bool enabled)
{
    Settings::chatAssistantSettings().enableChatTools.setValue(enabled);
    Settings::chatAssistantSettings().writeSettings();
}

bool ChatRootView::useThinking() const
{
    return Settings::chatAssistantSettings().enableThinkingMode();
}

void ChatRootView::setUseThinking(bool enabled)
{
    Settings::chatAssistantSettings().enableThinkingMode.setValue(enabled);
    Settings::chatAssistantSettings().writeSettings();
}

void ChatRootView::respondToPermission(const QString &requestId, const QString &optionId)
{
    m_controller->respondToPermission(requestId, optionId);
}

void ChatRootView::applyFileEdit(const QString &editId)
{
    if (refuseWhileReadOnly())
        return;

    m_fileEditController->applyFileEdit(editId);
}

void ChatRootView::rejectFileEdit(const QString &editId)
{
    if (refuseWhileReadOnly())
        return;

    m_fileEditController->rejectFileEdit(editId);
}

void ChatRootView::undoFileEdit(const QString &editId)
{
    if (refuseWhileReadOnly())
        return;

    m_fileEditController->undoFileEdit(editId);
}

void ChatRootView::openFileEditInEditor(const QString &editId)
{
    m_fileEditController->openFileEditInEditor(editId);
}

void ChatRootView::applyAllFileEditsForCurrentMessage()
{
    if (refuseWhileReadOnly())
        return;

    m_fileEditController->applyAllForCurrentMessage();
}

void ChatRootView::undoAllFileEditsForCurrentMessage()
{
    if (refuseWhileReadOnly())
        return;

    m_fileEditController->undoAllForCurrentMessage();
}

void ChatRootView::updateCurrentMessageEditsStats()
{
    m_fileEditController->updateStats();
}

int ChatRootView::currentMessageTotalEdits() const
{
    return m_fileEditController->totalEdits();
}

int ChatRootView::currentMessageAppliedEdits() const
{
    return m_fileEditController->appliedEdits();
}

int ChatRootView::currentMessagePendingEdits() const
{
    return m_fileEditController->pendingEdits();
}

int ChatRootView::currentMessageRejectedEdits() const
{
    return m_fileEditController->rejectedEdits();
}

QString ChatRootView::lastInfoMessage() const
{
    return m_lastInfoMessage;
}

bool ChatRootView::isThinkingSupport() const
{
    auto providerName = Settings::generalSettings().caProvider();
    auto provider = Providers::ProvidersManager::instance().getProviderByName(providerName);

    return provider && provider->capabilities().testFlag(Providers::ProviderCapability::Thinking);
}

bool ChatRootView::hasImageAttachments(const QStringList &attachments) const
{
    for (const QString &filePath : attachments) {
        if (isImageFile(filePath)) {
            return true;
        }
    }
    return false;
}

bool ChatRootView::isImageFile(const QString &filePath) const
{
    static const QSet<QString> imageExtensions = {"png", "jpg", "jpeg", "gif", "webp", "bmp", "svg"};

    QFileInfo fileInfo(filePath);
    return imageExtensions.contains(fileInfo.suffix().toLower());
}

void ChatRootView::loadAvailableConfigurations()
{
    m_configurationController->loadAvailableConfigurations();
}

void ChatRootView::applyConfiguration(const QString &configName)
{
    m_configurationController->applyConfiguration(configName);
}

void ChatRootView::handleAgentRequested(const Acp::AgentDefinition &agent)
{
    if (m_controller->boundAgentId() == agent.id)
        return;

    if (m_controller->conversationStarted()) {
        m_pendingAgent = agent;
        m_pendingLlmSwitch = false;
        emit chatTargetSwitchNeedsNewChat(agent.name);
        return;
    }

    bindAgent(agent);
}

void ChatRootView::handleLlmRequested()
{
    if (m_controller->boundAgentId().isEmpty())
        return;

    if (m_controller->conversationStarted()) {
        m_pendingAgent.reset();
        m_pendingLlmSwitch = true;
        emit chatTargetSwitchNeedsNewChat(tr("direct LLM chat"));
        return;
    }

    bindLlm();
}

void ChatRootView::confirmChatTargetSwitch()
{
    const auto agent = m_pendingAgent;
    const bool toLlm = m_pendingLlmSwitch;

    m_pendingAgent.reset();
    m_pendingLlmSwitch = false;

    if (!agent && !toLlm)
        return;

    clearMessages();

    if (agent)
        bindAgent(*agent);
    else
        bindLlm();
}

void ChatRootView::cancelChatTargetSwitch()
{
    m_pendingAgent.reset();
    m_pendingLlmSwitch = false;
    emit currentConfigurationChanged();
}

void ChatRootView::bindAgent(const Acp::AgentDefinition &agent)
{
    m_controller->bindToAgent(agent);
    m_configurationController->setBoundAgent(agent);
    emit isAgentBoundChanged();
}

void ChatRootView::bindLlm()
{
    m_controller->bindToLlm();
    m_configurationController->clearBoundAgent();
    emit isAgentBoundChanged();
}

bool ChatRootView::isAgentBound() const
{
    return !m_controller->boundAgentId().isEmpty();
}

QString ChatRootView::agentSessionIssue() const
{
    return m_agentSessionIssue;
}

bool ChatRootView::canStartNewAgentSession() const
{
    return m_agentSessionRecoverable;
}

void ChatRootView::setAgentSessionIssue(const QString &issue, bool recoverable)
{
    if (m_agentSessionIssue == issue && m_agentSessionRecoverable == recoverable)
        return;

    m_agentSessionIssue = issue;
    m_agentSessionRecoverable = recoverable;
    emit agentSessionIssueChanged();
}

void ChatRootView::startNewAgentSession()
{
    if (!m_agentSessionRecoverable)
        return;

    m_controller->startFreshAgentSession();
    setAgentSessionIssue(QString(), false);
}

bool ChatRootView::canHandOverSummary() const
{
    return m_agentSessionRecoverable && ChatCompressor::configurationIssue().isEmpty()
           && !m_controller->session()->rows().isEmpty();
}

QString ChatRootView::summaryHandoverTooltip() const
{
    if (!m_agentSessionRecoverable)
        return {};

    if (m_controller->session()->rows().isEmpty())
        return tr("There is nothing to summarise yet.");

    const QString issue = ChatCompressor::configurationIssue();
    if (!issue.isEmpty()) {
        return tr("A summary cannot be produced because %1. Start a new session without one, or "
                  "assign the chat feature in the settings.")
            .arg(issue);
    }

    return tr("Summarise this transcript and give it to the new session as context.");
}

void ChatRootView::startNewAgentSessionWithSummary()
{
    if (!canHandOverSummary())
        return;

    if (m_chatCompressor->isCompressing()) {
        m_lastErrorMessage = tr("A summary is already being prepared");
        emit lastErrorMessageChanged();
        return;
    }

    m_chatCompressor->startSummary(m_controller->session()->history());
}

void ChatRootView::restoreAgentBinding(const Acp::AgentBinding &binding)
{
    setAgentSessionIssue(QString(), false);
    m_quarantinedBinding = {};
    m_controller->releaseAgentSession();

    if (binding.isEmpty()) {
        bindLlm();
        return;
    }

    const auto agent = m_configurationController->agentById(binding.agentId);
    if (!agent || !agent->isLaunchable()) {
        m_quarantinedBinding = binding;
        bindLlm();
        m_configurationController->clearBoundAgent();
        setAgentSessionIssue(
            tr("This chat was held with the agent \"%1\", which is not available any more. "
               "The transcript is read-only, and the agent it names is kept so the chat can be "
               "continued once that agent is installed again.")
                .arg(binding.displayId()),
            false);
        return;
    }

    bindAgent(*agent);

    if (!binding.sessionId.isEmpty()) {
        m_controller->resumeAgentSession(binding.sessionId);
        return;
    }

    if (m_controller->conversationStarted()) {
        setAgentSessionIssue(
            tr("This chat records the agent \"%1\" but not a session to reopen, so the transcript "
               "is read-only. Start a new session to keep working with this agent — it will not "
               "have the context above.")
                .arg(binding.displayId()),
            true);
    }
}

QStringList ChatRootView::availableConfigurations() const
{
    return m_configurationController->availableConfigurations();
}

QString ChatRootView::currentConfiguration() const
{
    return m_configurationController->currentConfiguration();
}

void ChatRootView::loadAvailableAgentRoles()
{
    m_agentRoleController->loadAvailableRoles();
}

void ChatRootView::applyAgentRole(const QString &roleName)
{
    m_agentRoleController->applyRole(roleName);
}

QStringList ChatRootView::availableAgentRoles() const
{
    return m_agentRoleController->availableRoles();
}

QString ChatRootView::currentAgentRole() const
{
    return m_agentRoleController->currentRole();
}

QString ChatRootView::baseSystemPrompt() const
{
    return m_agentRoleController->baseSystemPrompt();
}

QString ChatRootView::currentAgentRoleDescription() const
{
    return m_agentRoleController->currentRoleDescription();
}

QString ChatRootView::currentAgentRoleSystemPrompt() const
{
    return m_agentRoleController->currentRoleSystemPrompt();
}

void ChatRootView::openAgentRolesSettings()
{
    m_agentRoleController->openSettings();
}

void ChatRootView::compressCurrentChat()
{
    if (refuseWhileReadOnly())
        return;

    if (isAgentBound()) {
        m_lastErrorMessage
            = tr("An agent conversation cannot be compressed: the summary would have no agent "
                 "session behind it.");
        emit lastErrorMessageChanged();
        return;
    }

    if (m_chatCompressor->isCompressing()) {
        m_lastErrorMessage = tr("Compression is already in progress");
        emit lastErrorMessageChanged();
        return;
    }

    if (m_recentFilePath.isEmpty()) {
        m_lastErrorMessage = tr("No chat file to compress. Please save the chat first.");
        emit lastErrorMessageChanged();
        return;
    }

    autosave();

    m_chatCompressor->startCompression(
        m_recentFilePath, m_controller->session()->history());
}

void ChatRootView::cancelCompression()
{
    m_chatCompressor->cancelCompression();
}

bool ChatRootView::isCompressing() const
{
    return m_chatCompressor->isCompressing();
}

} // namespace QodeAssist::Chat
