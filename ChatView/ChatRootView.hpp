/*
 * Copyright (C) 2024 Petr Mironychev
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

namespace QodeAssist::Chat {

class ChatRootView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QodeAssist::Chat::ChatModel *chatModel READ chatModel NOTIFY chatModelChanged FINAL)
    Q_PROPERTY(QString currentTemplate READ currentTemplate NOTIFY currentTemplateChanged FINAL)
    Q_PROPERTY(bool isSharingCurrentFile READ isSharingCurrentFile NOTIFY
                   isSharingCurrentFileChanged FINAL)
    Q_PROPERTY(QStringList attachmentFiles READ attachmentFiles NOTIFY attachmentFilesChanged FINAL)
    Q_PROPERTY(QStringList linkedFiles READ linkedFiles NOTIFY linkedFilesChanged FINAL)

    QML_ELEMENT

public:
    ChatRootView(QQuickItem *parent = nullptr);

    ChatModel *chatModel() const;
    QString currentTemplate() const;

    bool isSharingCurrentFile() const;

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

public slots:
    void sendMessage(const QString &message, bool sharingCurrentFile = false);
    void copyToClipboard(const QString &text);
    void cancelRequest();
    void clearAttachmentFiles();
    void clearLinkedFiles();

signals:
    void chatModelChanged();
    void currentTemplateChanged();
    void isSharingCurrentFileChanged();
    void attachmentFilesChanged();
    void linkedFilesChanged();

private:
    QString getChatsHistoryDir() const;
    QString getSuggestedFileName() const;

    ChatModel *m_chatModel;
    ClientInterface *m_clientInterface;
    QString m_currentTemplate;
    QString m_recentFilePath;
    QStringList m_attachmentFiles;
    QStringList m_linkedFiles;
};

} // namespace QodeAssist::Chat
