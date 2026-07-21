// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>

#include <QObject>
#include <QPointer>
#include <QString>

#include "acp/AgentBinding.hpp"
#include "session/ConversationHistory.hpp"

namespace QodeAssist::Session {
class Session;
}

namespace QodeAssist::Chat {

struct SerializationResult
{
    bool success{false};
    QString errorMessage;
    QString warningMessage;
};

class ChatFileStore : public QObject
{
    Q_OBJECT

public:
    explicit ChatFileStore(Session::Session *session, QObject *parent = nullptr);

    QString historyDir() const;
    QString suggestedFileName() const;
    QString autosaveFilePath(const QString &recentFilePath) const;
    QString autosaveFilePath(
        const QString &recentFilePath,
        const QString &firstMessage,
        bool hasImageAttachments) const;

    using BindingReader = std::function<Acp::AgentBinding()>;
    using BindingWriter = std::function<void(const Acp::AgentBinding &)>;

    void setBindingReader(BindingReader reader);
    void setBindingWriter(BindingWriter writer);

    SerializationResult save(const QString &filePath) const;
    SerializationResult load(const QString &filePath) const;

    void showSaveDialog();
    void showLoadDialog();
    void openHistoryFolder() const;

    static SerializationResult saveToFile(
        const Session::ConversationHistory &history,
        const Acp::AgentBinding &binding,
        const QString &filePath);
    static SerializationResult loadFromFile(
        Session::ConversationHistory &history,
        Acp::AgentBinding &binding,
        const QString &filePath);

    static QString getChatContentFolder(const QString &chatFilePath);
    static bool saveContentToStorage(
        const QString &chatFilePath,
        const QString &fileName,
        const QString &base64Data,
        QString &storedPath);
    static QString loadContentFromStorage(const QString &chatFilePath, const QString &storedPath);
    static QByteArray loadRawContentFromStorage(
        const QString &chatFilePath, const QString &storedPath);

signals:
    void saveRequested(const QString &filePath);
    void loadRequested(const QString &filePath);

private:
    QString generateChatFileName(const QString &shortMessage, const QString &dir) const;
    static bool ensureDirectoryExists(const QString &filePath);

    QPointer<Session::Session> m_session;
    BindingReader m_bindingReader;
    BindingWriter m_bindingWriter;
};

} // namespace QodeAssist::Chat
