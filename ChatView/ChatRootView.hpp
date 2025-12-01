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

#pragma once

#include <QQuickItem>

#include "ChatModel.hpp"
#include "ClientInterface.hpp"
#include "ChatFileManager.hpp"
#include "llmcore/PromptProviderChat.hpp"
#include <coreplugin/editormanager/editormanager.h>

namespace QodeAssist::Chat {

class ChatRootView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QodeAssist::Chat::ChatModel *chatModel READ chatModel NOTIFY chatModelChanged FINAL)
    Q_PROPERTY(QString currentTemplate READ currentTemplate NOTIFY currentTemplateChanged FINAL)
    Q_PROPERTY(bool isSyncOpenFiles READ isSyncOpenFiles NOTIFY isSyncOpenFilesChanged FINAL)
    Q_PROPERTY(QStringList attachmentFiles READ attachmentFiles NOTIFY attachmentFilesChanged FINAL)
    Q_PROPERTY(QStringList linkedFiles READ linkedFiles NOTIFY linkedFilesChanged FINAL)
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
    Q_PROPERTY(QVariantList activeRules READ activeRules NOTIFY activeRulesChanged FINAL)
    Q_PROPERTY(int activeRulesCount READ activeRulesCount NOTIFY activeRulesCountChanged FINAL)
    Q_PROPERTY(bool useTools READ useTools WRITE setUseTools NOTIFY useToolsChanged FINAL)
    Q_PROPERTY(bool useThinking READ useThinking WRITE setUseThinking NOTIFY useThinkingChanged FINAL)
    
    Q_PROPERTY(int currentMessageTotalEdits READ currentMessageTotalEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(int currentMessageAppliedEdits READ currentMessageAppliedEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(int currentMessagePendingEdits READ currentMessagePendingEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(int currentMessageRejectedEdits READ currentMessageRejectedEdits NOTIFY currentMessageEditsStatsChanged FINAL)
    Q_PROPERTY(bool isThinkingSupport READ isThinkingSupport NOTIFY isThinkingSupportChanged FINAL)
    Q_PROPERTY(QStringList availableConfigurations READ availableConfigurations NOTIFY availableConfigurationsChanged FINAL)
    Q_PROPERTY(QString currentConfiguration READ currentConfiguration NOTIFY currentConfigurationChanged FINAL)

    QML_ELEMENT

public:
    ChatRootView(QQuickItem *parent = nullptr);

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
    QStringList linkedFiles() const;

    Q_INVOKABLE void showAttachFilesDialog();
    Q_INVOKABLE void addFilesToAttachList(const QStringList &filePaths);
    Q_INVOKABLE void removeFileFromAttachList(int index);
    Q_INVOKABLE void showLinkFilesDialog();
    Q_INVOKABLE void addFilesToLinkList(const QStringList &filePaths);
    Q_INVOKABLE void removeFileFromLinkList(int index);
    Q_INVOKABLE QStringList convertUrlsToLocalPaths(const QVariantList &urls) const;
    Q_INVOKABLE void showAddImageDialog();
    Q_INVOKABLE bool isImageFile(const QString &filePath) const;
    Q_INVOKABLE void calculateMessageTokensCount(const QString &message);
    Q_INVOKABLE void setIsSyncOpenFiles(bool state);
    Q_INVOKABLE void openChatHistoryFolder();
    Q_INVOKABLE void openRulesFolder();
    Q_INVOKABLE void openSettings();

    Q_INVOKABLE void updateInputTokensCount();
    int inputTokensCount() const;

    bool isSyncOpenFiles() const;

    void onEditorAboutToClose(Core::IEditor *editor);
    void onAppendLinkFileFromEditor(Core::IEditor *editor);
    void onEditorCreated(Core::IEditor *editor, const Utils::FilePath &filePath);

    QString chatFileName() const;
    Q_INVOKABLE QString chatFilePath() const;
    void setRecentFilePath(const QString &filePath);
    bool shouldIgnoreFileForAttach(const Utils::FilePath &filePath);

    QString textFontFamily() const;
    QString codeFontFamily() const;

    int codeFontSize() const;
    int textFontSize() const;
    int textFormat() const;

    bool isRequestInProgress() const;
    void setRequestProgressStatus(bool state);

    QString lastErrorMessage() const;
    
    QVariantList activeRules() const;
    int activeRulesCount() const;
    Q_INVOKABLE QString getRuleContent(int index);
    Q_INVOKABLE void refreshRules();

    bool useTools() const;
    void setUseTools(bool enabled);
    bool useThinking() const;
    void setUseThinking(bool enabled);

    Q_INVOKABLE void applyFileEdit(const QString &editId);
    Q_INVOKABLE void rejectFileEdit(const QString &editId);
    Q_INVOKABLE void undoFileEdit(const QString &editId);
    Q_INVOKABLE void openFileEditInEditor(const QString &editId);
    
    Q_INVOKABLE void applyAllFileEditsForCurrentMessage();
    Q_INVOKABLE void undoAllFileEditsForCurrentMessage();
    Q_INVOKABLE void updateCurrentMessageEditsStats();

    Q_INVOKABLE void loadAvailableConfigurations();
    Q_INVOKABLE void applyConfiguration(const QString &configName);
    QStringList availableConfigurations() const;
    QString currentConfiguration() const;
    
    int currentMessageTotalEdits() const;
    int currentMessageAppliedEdits() const;
    int currentMessagePendingEdits() const;
    int currentMessageRejectedEdits() const;

    QString lastInfoMessage() const;

    bool isThinkingSupport() const;

public slots:
    void sendMessage(const QString &message);
    void copyToClipboard(const QString &text);
    void cancelRequest();
    void clearAttachmentFiles();
    void clearLinkedFiles();

signals:
    void chatModelChanged();
    void currentTemplateChanged();
    void attachmentFilesChanged();
    void linkedFilesChanged();
    void inputTokensCountChanged();
    void isSyncOpenFilesChanged();
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
    void activeRulesChanged();
    void activeRulesCountChanged();

    void useToolsChanged();
    void useThinkingChanged();
    void currentMessageEditsStatsChanged();

    void isThinkingSupportChanged();
    void availableConfigurationsChanged();
    void currentConfigurationChanged();

private:
    void updateFileEditStatus(const QString &editId, const QString &status);
    QString getChatsHistoryDir() const;
    QString getSuggestedFileName() const;
    QString generateChatFileName(const QString &shortMessage, const QString &dir) const;
    bool hasImageAttachments(const QStringList &attachments) const;

    ChatModel *m_chatModel;
    LLMCore::PromptProviderChat m_promptProvider;
    ClientInterface *m_clientInterface;
    ChatFileManager *m_fileManager;
    QString m_currentTemplate;
    QString m_recentFilePath;
    QStringList m_attachmentFiles;
    QStringList m_linkedFiles;
    int m_messageTokensCount{0};
    int m_inputTokensCount{0};
    bool m_isSyncOpenFiles;
    QList<Core::IEditor *> m_currentEditors;
    bool m_isRequestInProgress;
    QString m_lastErrorMessage;
    QVariantList m_activeRules;
    
    QString m_currentMessageRequestId;
    int m_currentMessageTotalEdits{0};
    int m_currentMessageAppliedEdits{0};
    int m_currentMessagePendingEdits{0};
    int m_currentMessageRejectedEdits{0};
    QString m_lastInfoMessage;
    
    QStringList m_availableConfigurations;
    QString m_currentConfiguration;
};

} // namespace QodeAssist::Chat
