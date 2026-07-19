// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "ChatSerializer.hpp"

namespace QodeAssist::Session {
class Session;
}

namespace QodeAssist::Chat {

class ChatHistoryStore : public QObject
{
    Q_OBJECT

public:
    explicit ChatHistoryStore(Session::Session *session, QObject *parent = nullptr);

    QString historyDir() const;
    QString suggestedFileName() const;
    QString autosaveFilePath(const QString &recentFilePath) const;
    QString autosaveFilePath(
        const QString &recentFilePath,
        const QString &firstMessage,
        bool hasImageAttachments) const;

    SerializationResult save(const QString &filePath) const;
    SerializationResult load(const QString &filePath) const;

    void showSaveDialog();
    void showLoadDialog();
    void openHistoryFolder() const;

signals:
    void saveRequested(const QString &filePath);
    void loadRequested(const QString &filePath);

private:
    QString generateChatFileName(const QString &shortMessage, const QString &dir) const;

    QPointer<Session::Session> m_session;
};

} // namespace QodeAssist::Chat
