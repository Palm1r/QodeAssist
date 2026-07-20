// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QVariantList>

#include "ChatController.hpp"
#include "ChatFileManager.hpp"
#include "ChatModel.hpp"
#include "templates/PromptProviderChat.hpp"
#include <utils/id.h>

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Chat {

class ChatCompressor;
class ChatConfigurationController;
class FileEditController;
class InputTokenCounter;
class ChatHistoryStore;
class SessionFileRegistry;

class ChatRootView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QodeAssist::Chat::ChatModel *chatModel READ chatModel NOTIFY chatModelChanged FINAL)
    Q_PROPERTY(QString currentTemplate READ currentTemplate NOTIFY currentTemplateChanged FINAL)
    Q_PROPERTY(QStringList attachmentFiles READ attachmentFiles NOTIFY attachmentFilesChanged FINAL)
    Q_PROPERTY(int inputTokensCount READ inputTokensCount NOTIFY inputTokensCountChanged FINAL)
    Q_PROPERTY(QString chatFileName READ chatFileName NOTIFY chatFileNameChanged FINAL)
    Q_PROPERTY(QString textFontFamily READ textFontFamily NOTIFY textFamilyChanged FINAL)
    Q_PROPERTY(QString codeFontFamily READ codeFontFamily NOTIFY codeFamilyChanged FINAL)
    Q_PROPERTY(int codeFontSize READ codeFontSize NOTIFY codeFontSizeChanged FINAL)
    Q_PROPERTY(int textFontSize READ textFontSize NOTIFY textFontSizeChanged FINAL)
    Q_PROPERTY(int textFormat READ textFormat NOTIFY textFormatChanged FINAL)
    Q_PROPERTY(bool isRequestInProgress READ isRequestInProgress NOTIFY isRequestInProgressChanged FINAL)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorMessageChanged FINAL)
    Q_PROPERTY(QString lastInfoMessage READ lastInfoMessage NOTIFY lastInfoMessageChanged FINAL)
    Q_PROPERTY(bool useTools READ useTools WRITE setUseTools NOTIFY useToolsChanged FINAL)
    Q_PROPERTY(bool useThinking READ useThinking WRITE setUseThinking NOTIFY useThinkingChanged FINAL)
    Q_PROPERTY(QString sendShortcutText READ sendShortcutText NOTIFY sendShortcutTextChanged FINAL)
    
    Q_PROPERTY(int currentMessageTotalEdits READ currentMessageTotalEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(int currentMessageAppliedEdits READ currentMessageAppliedEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(int currentMessagePendingEdits READ currentMessagePendingEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(int currentMessageRejectedEdits READ currentMessageRejectedEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(bool isThinkingSupport READ isThinkingSupport NOTIFY isThinkingSupportChanged FINAL)
    Q_PROPERTY(bool isAgentBound READ isAgentBound NOTIFY isAgentBoundChanged FINAL)
    Q_PROPERTY(QString agentSessionIssue READ agentSessionIssue NOTIFY agentSessionIssueChanged FINAL)
    Q_PROPERTY(bool canStartNewAgentSession READ canStartNewAgentSession NOTIFY
                   agentSessionIssueChanged FINAL)
    Q_PROPERTY(bool canHandOverSummary READ canHandOverSummary NOTIFY agentSessionIssueChanged FINAL)
    Q_PROPERTY(QString summaryHandoverTooltip READ summaryHandoverTooltip NOTIFY
                   agentSessionIssueChanged FINAL)
    Q_PROPERTY(QStringList availableConfigurations READ availableConfigurations NOTIFY availableConfigurationsChanged FINAL)
    Q_PROPERTY(QString currentConfiguration READ currentConfiguration NOTIFY currentConfigurationChanged FINAL)
    Q_PROPERTY(QString baseSystemPrompt READ baseSystemPrompt NOTIFY baseSystemPromptChanged FINAL)
    Q_PROPERTY(bool isCompressing READ isCompressing NOTIFY isCompressingChanged FINAL)
    Q_PROPERTY(bool isInEditor READ isInEditor NOTIFY isInEditorChanged FINAL)
    Q_PROPERTY(QString chatTitle READ chatTitle NOTIFY chatTitleChanged FINAL)

    QML_ELEMENT

public:
    ChatRootView(QQuickItem *parent = nullptr);
    ~ChatRootView() override;

    ChatModel *chatModel() const;
    QString currentTemplate() const;

    void saveHistory(const QString &filePath);
    void loadHistory(const QString &filePath);

    Q_INVOKABLE void showSaveDialog();
    Q_INVOKABLE void showLoadDialog();

    void autosave();
    QString getAutosaveFilePath() const;
    QString getAutosaveFilePath(const QString &firstMessage, const QStringList &attachments) const;

    QStringList attachmentFiles() const;

    Q_INVOKABLE void showAttachFilesDialog();
    Q_INVOKABLE void addFilesToAttachList(const QStringList &filePaths);
    Q_INVOKABLE void removeFileFromAttachList(int index);
    Q_INVOKABLE QStringList convertUrlsToLocalPaths(const QVariantList &urls) const;
    Q_INVOKABLE void showAddImageDialog();
    Q_INVOKABLE bool isImageFile(const QString &filePath) const;
    Q_INVOKABLE void calculateMessageTokensCount(const QString &message);
    Q_INVOKABLE bool isSendShortcut(int key, int modifiers) const;
    QString sendShortcutText() const;
    Q_INVOKABLE void openChatHistoryFolder();
    Q_INVOKABLE void openSettings();

    Q_INVOKABLE void openFileInEditor(const QString &filePath);

    Q_INVOKABLE void relocateToSplit();
    Q_INVOKABLE void relocateToWindow();

    void consumePendingChatFile();

    Q_INVOKABLE void updateInputTokensCount();
    int inputTokensCount() const;

    QString chatFileName() const;
    Q_INVOKABLE QString chatFilePath() const;
    void setRecentFilePath(const QString &filePath);

    QString textFontFamily() const;
    QString codeFontFamily() const;

    int codeFontSize() const;
    int textFontSize() const;
    int textFormat() const;

    bool isRequestInProgress() const;
    void setRequestProgressStatus(bool state);

    QString lastErrorMessage() const;

    Q_INVOKABLE QVariantList searchSkills(const QString &query) const;

    bool useTools() const;
    void setUseTools(bool enabled);
    bool useThinking() const;
    void setUseThinking(bool enabled);

    Q_INVOKABLE void respondToPermission(const QString &requestId, const QString &optionId);

    Q_INVOKABLE void applyFileEdit(const QString &editId);
    Q_INVOKABLE void rejectFileEdit(const QString &editId);
    Q_INVOKABLE void undoFileEdit(const QString &editId);
    Q_INVOKABLE void openFileEditInEditor(const QString &editId);
    
    Q_INVOKABLE void applyAllFileEditsForCurrentMessage();
    Q_INVOKABLE void undoAllFileEditsForCurrentMessage();
    Q_INVOKABLE void updateCurrentMessageEditsStats();

    Q_INVOKABLE void loadAvailableConfigurations();
    Q_INVOKABLE void applyConfiguration(const QString &configName);
    Q_INVOKABLE void confirmChatTargetSwitch();
    Q_INVOKABLE void cancelChatTargetSwitch();
    QStringList availableConfigurations() const;
    QString currentConfiguration() const;

    Q_INVOKABLE void compressCurrentChat();
    Q_INVOKABLE void cancelCompression();

    QString baseSystemPrompt() const;

    int currentMessageTotalEdits() const;
    int currentMessageAppliedEdits() const;
    int currentMessagePendingEdits() const;
    int currentMessageRejectedEdits() const;

    QString lastInfoMessage() const;

    bool isThinkingSupport() const;
    bool isAgentBound() const;
    QString agentSessionIssue() const;
    bool canStartNewAgentSession() const;
    bool canHandOverSummary() const;
    QString summaryHandoverTooltip() const;
    Q_INVOKABLE void startNewAgentSession();
    Q_INVOKABLE void startNewAgentSessionWithSummary();
    void setAgentSessionIssue(const QString &issue, bool recoverable);
    void restoreAgentBinding(const Acp::AgentBinding &binding);
    bool refuseWhileReadOnly();
    
    bool isCompressing() const;

    bool isInEditor() const;
    void setInEditor(bool value);

    QString chatTitle() const;

    Q_INVOKABLE void requestNewChat();

public slots:
    void sendMessage(const QString &message);
    void copyToClipboard(const QString &text);
    void cancelRequest();
    void clearAttachmentFiles();
    void clearMessages();
    void resetChatToMessage(int index);

signals:
    void chatModelChanged();
    void currentTemplateChanged();
    void attachmentFilesChanged();
    void inputTokensCountChanged();
    void chatFileNameChanged();
    void textFamilyChanged();
    void codeFamilyChanged();
    void codeFontSizeChanged();
    void textFontSizeChanged();
    void textFormatChanged();
    void chatRequestStarted();
    void isRequestInProgressChanged();

    void lastErrorMessageChanged();
    void lastInfoMessageChanged();
    void sendShortcutTextChanged();

    void useToolsChanged();
    void useThinkingChanged();
    void currentMessageEditsStatsChanged();

    void isThinkingSupportChanged();
    void isAgentBoundChanged();
    void agentSessionIssueChanged();
    void availableConfigurationsChanged();
    void currentConfigurationChanged();
    void chatTargetSwitchNeedsNewChat(const QString &targetName);

    void baseSystemPromptChanged();

    void isCompressingChanged();
    void compressionCompleted(const QString &compressedChatPath);
    void compressionFailed(const QString &error);

    void isInEditorChanged();
    void chatTitleChanged();

    void closeHostRequested();

private:
    QString computeChatTitle() const;
    void triggerOpenChatCommand(Utils::Id commandId);
    void handleAgentRequested(const Acp::AgentDefinition &agent);
    void handleLlmRequested();
    void bindAgent(const Acp::AgentDefinition &agent);
    void bindLlm();
    void handOffSession();
    bool deferSendForAutoCompress(
        const QString &message, const QStringList &attachments, bool useTools, bool useThinking);
    void dispatchSend(
        const QString &message, const QStringList &attachments, bool useTools, bool useThinking);
    bool hasImageAttachments(const QStringList &attachments) const;

    SessionFileRegistry *sessionFileRegistry() const;
    Skills::SkillsManager *skillsManager() const;

    ChatModel *m_chatModel;
    Templates::PromptProviderChat m_promptProvider;
    ChatController *m_controller;
    ChatFileManager *m_fileManager;
    QString m_currentTemplate;
    QString m_recentFilePath;
    QStringList m_attachmentFiles;

    struct PendingSend {
        QString message;
        QStringList attachments;
        bool useTools = false;
        bool useThinking = false;
        bool active = false;
    };
    PendingSend m_pendingSend;
    bool m_isInEditor = false;
    mutable QString m_cachedChatTitle;
    QString m_agentSuggestedTitle;
    QString m_agentSessionIssue;
    bool m_agentSessionRecoverable = false;
    Acp::AgentBinding m_quarantinedBinding;
    bool m_isRequestInProgress;
    QString m_lastErrorMessage;

    QString m_lastInfoMessage;

    ChatCompressor *m_chatCompressor;
    ChatConfigurationController *m_configurationController;
    FileEditController *m_fileEditController;
    InputTokenCounter *m_tokenCounter;
    ChatHistoryStore *m_historyStore;
    mutable QPointer<SessionFileRegistry> m_sessionFileRegistry;
    mutable bool m_sessionFileRegistryResolved = false;
    mutable QPointer<Skills::SkillsManager> m_skillsManager;
    mutable bool m_skillsManagerResolved = false;
    std::optional<Acp::AgentDefinition> m_pendingAgent;
    bool m_pendingLlmSwitch = false;
};

} // namespace QodeAssist::Chat
