// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

namespace QodeAssist {
class Session;
}

namespace QodeAssist::Chat {

// Shared registry mapping each chat (autosave) file to the live Session that owns it, so a
// file is busy only while its owning Session is alive (a destroyed Session frees it — the
// QPointer goes null). Keeps two chat views from autosaving into the same path.
class SessionFileRegistry : public QObject
{
    Q_OBJECT

public:
    explicit SessionFileRegistry(QObject *parent = nullptr);
    ~SessionFileRegistry() override;

    bool isLocked(const QString &path) const;
    bool isLockedByOther(const QString &path, QodeAssist::Session *self) const;
    bool lock(const QString &path, QodeAssist::Session *owner);
    void release(const QString &path);

    QString uniqueFreePath(const QString &desiredPath) const;

    // Handoff slot for relocating a live chat between hosts (split <-> window): the source
    // chat stores its history file here, the freshly created host picks it up exactly once.
    void setPendingChatFile(const QString &path);
    QString takePendingChatFile();

private:
    QHash<QString, QPointer<QodeAssist::Session>> m_locks;
    QString m_pendingChatFile;
};

} // namespace QodeAssist::Chat
