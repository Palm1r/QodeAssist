// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

namespace QodeAssist::Context {
class ContextManager;
}

namespace QodeAssist::Session {
class Session;
}

namespace QodeAssist::Chat {

class InputTokenCounter : public QObject
{
    Q_OBJECT

public:
    InputTokenCounter(
        Session::Session *session,
        Context::ContextManager *contextManager,
        QObject *parent = nullptr);

    int inputTokens() const;

    void setMessage(const QString &message);
    void setAttachments(const QStringList &attachments);
    void recompute();
    void recomputeSoon();

    void recordSent();
    void recordServerUsage(int promptTokens);

signals:
    void inputTokensChanged();

private:
    struct CachedFileTokens
    {
        QDateTime modified;
        int tokens = 0;
    };

    void rewireToolsChangedConnection();
    int estimateFileTokens(const QStringList &paths);

    QPointer<Session::Session> m_session;
    Context::ContextManager *m_contextManager;
    QMetaObject::Connection m_toolsChangedConn;
    QTimer m_recomputeTimer;
    QHash<QString, CachedFileTokens> m_fileTokens;

    QStringList m_attachments;
    int m_messageTokens{0};
    int m_inputTokens{0};
    int m_lastSentEstimate{0};
    double m_calibrationFactor{1.0};
};

} // namespace QodeAssist::Chat
