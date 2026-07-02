// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

namespace QodeAssist {
class SessionManager;
class Session;
class ConversationHistory;
} // namespace QodeAssist

namespace QodeAssist::Chat {

class ChatCompressor : public QObject
{
    Q_OBJECT

public:
    explicit ChatCompressor(QObject *parent = nullptr);

    void setSessionManager(SessionManager *sessionManager);
    void setActiveAgent(const QString &agentName);

    void startCompression(const QString &chatFilePath, ConversationHistory *sourceHistory);

    bool isCompressing() const;
    void cancelCompression();

signals:
    void compressionStarted();
    void compressionCompleted(const QString &compressedChatPath);
    void compressionFailed(const QString &error);

private:
    void onCompressionFinished(const QString &requestId);
    void onCompressionFailed(const QString &requestId, const QString &error);

    QString createCompressedChatPath(const QString &originalPath) const;
    bool createCompressedChatFile(
        const QString &sourcePath, const QString &destPath, const QString &summary);
    void cleanupState();
    void handleCompressionError(const QString &error);

    bool m_isCompressing = false;
    QString m_currentRequestId;
    QString m_originalChatPath;
    QPointer<SessionManager> m_sessionManager;
    QString m_activeAgent;
    QPointer<Session> m_session;
};

} // namespace QodeAssist::Chat
