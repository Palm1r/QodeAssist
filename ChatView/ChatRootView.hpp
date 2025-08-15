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
    Q_PROPERTY(
        bool isRequestInProgress READ isRequestInProgress NOTIFY isRequestInProgressChanged FINAL)

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

    QStringList attachmentFiles() const;
    QStringList linkedFiles() const;

    Q_INVOKABLE void showAttachFilesDialog();
    Q_INVOKABLE void removeFileFromAttachList(int index);
    Q_INVOKABLE void showLinkFilesDialog();
    Q_INVOKABLE void removeFileFromLinkList(int index);
    Q_INVOKABLE void calculateMessageTokensCount(const QString &message);
    Q_INVOKABLE void setIsSyncOpenFiles(bool state);
    Q_INVOKABLE void openChatHistoryFolder();

    Q_INVOKABLE void updateInputTokensCount();
    int inputTokensCount() const;

    bool isSyncOpenFiles() const;

    void onEditorAboutToClose(Core::IEditor *editor);
    void onAppendLinkFileFromEditor(Core::IEditor *editor);
    void onEditorCreated(Core::IEditor *editor, const Utils::FilePath &filePath);

    QString chatFileName() const;
    void setRecentFilePath(const QString &filePath);
    bool shouldIgnoreFileForAttach(const Utils::FilePath &filePath);

    QString textFontFamily() const;
    QString codeFontFamily() const;

    int codeFontSize() const;
    int textFontSize() const;
    int textFormat() const;

    bool isRequestInProgress() const;
    void setRequestProgressStatus(bool state);

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

private:
    QString getChatsHistoryDir() const;
    QString getSuggestedFileName() const;

    ChatModel *m_chatModel;
    LLMCore::PromptProviderChat m_promptProvider;
    ClientInterface *m_clientInterface;
    QString m_currentTemplate;
    QString m_recentFilePath;
    QStringList m_attachmentFiles;
    QStringList m_linkedFiles;
    int m_messageTokensCount{0};
    int m_inputTokensCount{0};
    bool m_isSyncOpenFiles;
    QList<Core::IEditor *> m_currentEditors;
    bool m_isRequestInProgress;
};

} // namespace QodeAssist::Chat
