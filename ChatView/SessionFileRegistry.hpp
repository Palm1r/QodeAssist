// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QSet>
#include <QString>

namespace QodeAssist::Chat {

// Shared registry of chat session (autosave) file paths that are currently held by a live
// chat instance. Lets every chat view — bottom pane, navigation panel, editor split — claim
// a unique history file so two sessions never autosave into the same path.
class SessionFileRegistry : public QObject
{
    Q_OBJECT

public:
    explicit SessionFileRegistry(QObject *parent = nullptr);

    bool isLocked(const QString &path) const;
    bool lock(const QString &path);
    void release(const QString &path);

    QString uniqueFreePath(const QString &desiredPath) const;

    // Handoff slot for relocating a live chat between hosts (split <-> window): the source
    // chat stores its history file here, the freshly created host picks it up exactly once.
    void setPendingChatFile(const QString &path);
    QString takePendingChatFile();

private:
    QSet<QString> m_lockedPaths;
    QString m_pendingChatFile;
};

} // namespace QodeAssist::Chat
