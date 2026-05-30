// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QStringList>

namespace QodeAssist {
class ConversationHistory;
}

namespace QodeAssist::Context {
class ContextManager;
}

namespace QodeAssist::Chat {

class InputTokenCounter : public QObject
{
    Q_OBJECT

public:
    InputTokenCounter(
        ConversationHistory *history,
        Context::ContextManager *contextManager,
        QObject *parent = nullptr);

    int inputTokens() const;

    void setMessage(const QString &message);
    void setAttachments(const QStringList &attachments);
    void setLinkedFiles(const QStringList &linkedFiles);
    void recompute();

    void recordSent();
    void recordServerUsage(int promptTokens);

signals:
    void inputTokensChanged();

private:
    ConversationHistory *m_history;
    Context::ContextManager *m_contextManager;

    QStringList m_attachments;
    QStringList m_linkedFiles;
    int m_messageTokens{0};
    int m_inputTokens{0};
    int m_lastSentEstimate{0};
    double m_calibrationFactor{1.0};
};

} // namespace QodeAssist::Chat
