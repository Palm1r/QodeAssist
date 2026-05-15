// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace QodeAssist::Chat {

class ChatModel;

class FileEditController : public QObject
{
    Q_OBJECT

public:
    explicit FileEditController(ChatModel *chatModel, QObject *parent = nullptr);

    void setCurrentRequestId(const QString &requestId);
    void clearCurrentRequestId();

    int totalEdits() const;
    int appliedEdits() const;
    int pendingEdits() const;
    int rejectedEdits() const;

    void applyFileEdit(const QString &editId);
    void rejectFileEdit(const QString &editId);
    void undoFileEdit(const QString &editId);
    void openFileEditInEditor(const QString &editId);

    void applyAllForCurrentMessage();
    void undoAllForCurrentMessage();
    void updateStats();

signals:
    void statsChanged();
    void infoMessage(const QString &message);
    void errorOccurred(const QString &error);

private:
    void updateFileEditStatus(const QString &editId, const QString &status);

    ChatModel *m_chatModel;
    QString m_currentRequestId;
    int m_totalEdits{0};
    int m_appliedEdits{0};
    int m_pendingEdits{0};
    int m_rejectedEdits{0};
};

} // namespace QodeAssist::Chat
