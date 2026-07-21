// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatRootView.hpp"

#include <QAction>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMessageBox>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTextStream>
#include <QUrl>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectmanager.h>
#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

#include "plugin/QodeAssistConstants.hpp"

#include "ChatAssistantSettings.hpp"
#include "ChatConfigurationController.hpp"
#include "acp/AgentCatalogStore.hpp"
#include "ChatCompressor.hpp"
#include "ChatFileStore.hpp"
#include "FileEditController.hpp"
#include "GeneralSettings.hpp"
#include "InputTokenCounter.hpp"
#include "SettingsConstants.hpp"
#include "Logger.hpp"
#include "SessionFileRegistry.hpp"
#include "context/ContextManager.hpp"
#include "TurnContextAdapters.hpp"
#include "ProjectSettings.hpp"
#include "SkillsSettings.hpp"
#include "skills/SkillsManager.hpp"

namespace QodeAssist::Chat {

namespace {
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
    , m_attachmentStaging(new AttachmentStaging(this))
    , m_isRequestInProgress(false)
    , m_chatCompressor(new ChatCompressor(this))
    , m_configurationController(new ChatConfigurationController(this))
    , m_fileEditController(new FileEditController(this))
    , m_tokenCounter(new InputTokenCounter(
          m_controller->session(), m_controller->contextManager(), this))
    , m_historyStore(new ChatFileStore(m_controller->session(), this))
{
    m_coordinator = new ConversationCoordinator({m_controller, this, this, this}, this);

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
        m_coordinator,
        &ConversationCoordinator::chooseAgent);
    connect(
        m_configurationController,
        &ChatConfigurationController::llmRequested,
        m_coordinator,
        &ConversationCoordinator::chooseLlm);

    connect(
        m_coordinator,
        &ConversationCoordinator::switchConfirmationNeeded,
        this,
        &ChatRootView::chatTargetSwitchNeedsNewChat);
    connect(
        m_coordinator,
        &ConversationCoordinator::switchCancelled,
        this,
        &ChatRootView::currentConfigurationChanged);
    connect(
        m_coordinator,
        &ConversationCoordinator::boundToAgent,
        this,
        [this](const Acp::AgentDefinition &agent) {
            m_configurationController->setBoundAgent(agent);
            emit isAgentBoundChanged();
        });
    connect(m_coordinator, &ConversationCoordinator::boundToLlm, this, [this] {
        m_configurationController->clearBoundAgent();
        emit isAgentBoundChanged();
    });
    connect(
        m_coordinator,
        &ConversationCoordinator::sessionIssueChanged,
        this,
        &ChatRootView::agentSessionIssueChanged);
    connect(
        m_coordinator,
        &ConversationCoordinator::errorSurfaced,
        this,
        [this](const QString &message) {
            m_lastErrorMessage = message;
            emit lastErrorMessageChanged();
        });

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
        m_coordinator->conversationReset();
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
        &ChatController::sessionInfoReceived,
        m_coordinator,
        &ConversationCoordinator::titleSuggested);
    connect(m_coordinator, &ConversationCoordinator::agentTitleChanged, this, maybeEmitTitle);

    connect(
        m_controller,
        &ChatController::agentSessionUnavailable,
        m_coordinator,
        &ConversationCoordinator::agentSessionUnavailable);

    m_historyStore->setBindingReader([this] { return m_coordinator->bindingForSave(); });
    m_historyStore->setBindingWriter(
        [this](const Acp::AgentBinding &binding) { m_coordinator->restoreAgentBinding(binding); });
    connect(m_chatModel, &QAbstractItemModel::modelReset, this, maybeEmitTitle);
    connect(m_chatModel, &QAbstractItemModel::rowsInserted, this, maybeEmitTitle);
    connect(m_chatModel, &QAbstractItemModel::rowsRemoved, this, maybeEmitTitle);

    connect(this, &ChatRootView::isAgentBoundChanged, this, &ChatRootView::shrinkContextStateChanged);
    connect(
        m_controller,
        &ChatController::agentCommandsChanged,
        this,
        &ChatRootView::slashCommandsChanged);
    connect(
        this, &ChatRootView::agentSessionIssueChanged, this, &ChatRootView::shrinkContextStateChanged);
    connect(
        m_chatModel,
        &QAbstractItemModel::modelReset,
        this,
        &ChatRootView::shrinkContextStateChanged);
    connect(
        m_chatModel,
        &QAbstractItemModel::rowsInserted,
        this,
        &ChatRootView::shrinkContextStateChanged);
    connect(
        m_chatModel,
        &QAbstractItemModel::rowsRemoved,
        this,
        &ChatRootView::shrinkContextStateChanged);
    connect(
        &Settings::generalSettings().caProvider,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::shrinkContextStateChanged);
    connect(
        &Settings::generalSettings().caModel,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::shrinkContextStateChanged);
    connect(
        &Settings::generalSettings().caTemplate,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::shrinkContextStateChanged);
    connect(
        &Settings::generalSettings().caUrl,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::shrinkContextStateChanged);
    connect(this, &ChatRootView::attachmentFilesChanged, this, [this]() {
        m_tokenCounter->setAttachments(m_attachmentFiles);
    });
    connect(
        m_tokenCounter,
        &InputTokenCounter::inputTokensChanged,
        this,
        &ChatRootView::inputTokensCountChanged);
    connect(
        &Settings::chatAssistantSettings().systemPrompt,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::baseSystemPromptChanged);

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
        m_historyStore, &ChatFileStore::saveRequested, this, &ChatRootView::saveHistory);
    connect(
        m_historyStore, &ChatFileStore::loadRequested, this, &ChatRootView::loadHistory);

    connect(m_attachmentStaging, &AttachmentStaging::fileOperationFailed, this, [this](const QString &error) {
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

        m_coordinator->compressionSettled();
    });

    connect(
        m_chatCompressor,
        &ChatCompressor::compressingChanged,
        this,
        &ChatRootView::isCompressingChanged);

    connect(
        m_chatCompressor,
        &ChatCompressor::summaryReady,
        m_coordinator,
        &ConversationCoordinator::summaryProduced);

    connect(m_chatCompressor, &ChatCompressor::compressionFailed, this, [this](const QString &error) {
        emit isCompressingChanged();
        m_lastErrorMessage = error;
        emit lastErrorMessageChanged();
        emit compressionFailed(error);

        m_coordinator->compressionSettled();
    });
}

ChatRootView::~ChatRootView()
{
    if (m_sessionFileRegistry && !m_recentFilePath.isEmpty()) {
        m_sessionFileRegistry->release(m_recentFilePath);
    }
}

void ChatRootView::componentComplete()
{
    QQuickItem::componentComplete();

    if (auto context = qmlContext(this)) {
        if (auto *store = qobject_cast<Acp::AgentCatalogStore *>(
                context->contextProperty("agentCatalog").value<QObject *>())) {
            m_configurationController->setAgentCatalog(store);
        }
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

QVariantList ChatRootView::searchSlashCommands(const QString &query) const
{
    if (isAgentBound())
        return searchAgentCommands(query);
    return searchSkills(query);
}

QVariantList ChatRootView::searchAgentCommands(const QString &query) const
{
    QVariantList results;
    const QString source = m_controller->boundAgentName();
    const QString needle = query.trimmed().toLower();

    const QList<LLMQore::Acp::AvailableCommand> commands = m_controller->agentCommands();
    for (const LLMQore::Acp::AvailableCommand &command : commands) {
        if (!needle.isEmpty() && !command.name.toLower().contains(needle)
            && !command.description.toLower().contains(needle)) {
            continue;
        }
        results.append(
            QVariantMap{
                {QStringLiteral("name"), command.name},
                {QStringLiteral("description"), command.description},
                {QStringLiteral("inputHint"), command.inputHint},
                {QStringLiteral("source"), source}});
    }
    return results;
}

QVariantList ChatRootView::searchSkills(const QString &query) const
{
    QVariantList results;
    auto *manager = skillsManager();
    if (!manager || !Settings::skillsSettings().enableSkills())
        return results;

    auto *project = activeProject();
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
        results.append(
            QVariantMap{
                {QStringLiteral("name"), skill.name},
                {QStringLiteral("description"), skill.description},
                {QStringLiteral("inputHint"), QString()},
                {QStringLiteral("source"), tr("QodeAssist")}});
    }
    return results;
}

ChatModel *ChatRootView::chatModel() const
{
    return m_chatModel;
}

void ChatRootView::sendMessage(const QString &message)
{
    const QStringList attachments = m_attachmentFiles;
    m_coordinator->requestSend(message, attachments);
}

bool ChatRootView::autoCompressEnabled() const
{
    return Settings::chatAssistantSettings().autoCompress();
}

int ChatRootView::autoCompressThreshold() const
{
    return Settings::chatAssistantSettings().autoCompressThreshold();
}

int ChatRootView::estimatedNextTokens() const
{
    return m_tokenCounter->inputTokens();
}

bool ChatRootView::prepareChatFileForCompression(
    const QString &message, const QStringList &attachments)
{
    if (!m_recentFilePath.isEmpty())
        return true;

    const QString filePath = getAutosaveFilePath(message, attachments);
    if (filePath.isEmpty())
        return false;

    setRecentFilePath(filePath);
    LOG_MESSAGE(QString("Set chat file path for new chat (auto-compress): %1").arg(filePath));
    return true;
}

void ChatRootView::dispatch(const QString &message, const QStringList &attachments)
{
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
    m_controller->sendMessage(message, attachments);

    m_attachmentStaging->clearIntermediateStorage();
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
    m_attachmentStaging->clearIntermediateStorage();
}

void ChatRootView::clearMessages()
{
    m_controller->clearConversation();
}

void ChatRootView::resetChatToMessage(int index)
{
    if (m_coordinator->refuseWhileReadOnly())
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

    if (!m_coordinator->hasDeferredSend())
        m_attachmentStaging->clearIntermediateStorage();
    m_attachmentFiles.clear();
    emit attachmentFilesChanged();

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

    const QStringList processedPaths = m_attachmentStaging->processDroppedFiles(filePaths);

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

void ChatRootView::openChatHistoryFolder()
{
    m_historyStore->openHistoryFolder();
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
    if (!m_coordinator->agentTitle().isEmpty())
        return m_coordinator->agentTitle();

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
    m_attachmentStaging->setChatFilePath(filePath);
    emit chatFileNameChanged();
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

void ChatRootView::respondToPermission(const QString &requestId, const QString &optionId)
{
    m_controller->respondToPermission(requestId, optionId);
}

void ChatRootView::applyFileEdit(const QString &editId)
{
    if (m_coordinator->refuseWhileReadOnly())
        return;

    m_fileEditController->applyFileEdit(editId);
}

void ChatRootView::rejectFileEdit(const QString &editId)
{
    if (m_coordinator->refuseWhileReadOnly())
        return;

    m_fileEditController->rejectFileEdit(editId);
}

void ChatRootView::undoFileEdit(const QString &editId)
{
    if (m_coordinator->refuseWhileReadOnly())
        return;

    m_fileEditController->undoFileEdit(editId);
}

void ChatRootView::openFileEditInEditor(const QString &editId)
{
    m_fileEditController->openFileEditInEditor(editId);
}

void ChatRootView::applyAllFileEditsForCurrentMessage()
{
    if (m_coordinator->refuseWhileReadOnly())
        return;

    m_fileEditController->applyAllForCurrentMessage();
}

void ChatRootView::undoAllFileEditsForCurrentMessage()
{
    if (m_coordinator->refuseWhileReadOnly())
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

void ChatRootView::confirmChatTargetSwitch()
{
    m_coordinator->confirmSwitch();
}

void ChatRootView::cancelChatTargetSwitch()
{
    m_coordinator->cancelSwitch();
}

bool ChatRootView::isAgentBound() const
{
    return !m_controller->boundAgentId().isEmpty();
}

QString ChatRootView::agentSessionIssue() const
{
    return m_coordinator->sessionIssue();
}

bool ChatRootView::canStartNewAgentSession() const
{
    return m_coordinator->canStartFreshSession();
}

void ChatRootView::startNewAgentSession()
{
    m_coordinator->startFreshSession();
}

bool ChatRootView::canHandOverSummary() const
{
    return m_coordinator->canHandOverSummary();
}

QString ChatRootView::summaryHandoverTooltip() const
{
    return m_coordinator->summaryHandoverTooltip();
}

void ChatRootView::startNewAgentSessionWithSummary()
{
    m_coordinator->handOverSummary();
}

std::optional<Acp::AgentDefinition> ChatRootView::agentById(const QString &agentId) const
{
    return m_configurationController->agentById(agentId);
}

QString ChatRootView::compressionConfigurationIssue() const
{
    return ChatCompressor::configurationIssue();
}

bool ChatRootView::isCompressionRunning() const
{
    return m_chatCompressor->isCompressing();
}

void ChatRootView::startTranscriptSummary()
{
    m_chatCompressor->startSummary(m_controller->session()->history());
}

void ChatRootView::startCompression()
{
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

    m_chatCompressor->startCompression(m_recentFilePath, m_controller->session()->history());
}

QStringList ChatRootView::availableConfigurations() const
{
    return m_configurationController->availableConfigurations();
}

QString ChatRootView::currentConfiguration() const
{
    return m_configurationController->currentConfiguration();
}

QString ChatRootView::baseSystemPrompt() const
{
    return Settings::chatAssistantSettings().systemPrompt();
}

void ChatRootView::compressCurrentChat()
{
    m_coordinator->shrinkContext();
}

bool ChatRootView::canShrinkContext() const
{
    return m_coordinator->canShrinkContext();
}

QString ChatRootView::shrinkContextTooltip() const
{
    return m_coordinator->shrinkContextTooltip();
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
