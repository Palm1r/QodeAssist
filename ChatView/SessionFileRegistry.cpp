// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SessionFileRegistry.hpp"

#include <utility>

#include <QFileInfo>

namespace QodeAssist::Chat {

SessionFileRegistry::SessionFileRegistry(QObject *parent)
    : QObject(parent)
{}

bool SessionFileRegistry::isLocked(const QString &path) const
{
    return !path.isEmpty() && m_lockedPaths.contains(path);
}

bool SessionFileRegistry::lock(const QString &path)
{
    if (path.isEmpty() || m_lockedPaths.contains(path)) {
        return false;
    }
    m_lockedPaths.insert(path);
    return true;
}

void SessionFileRegistry::release(const QString &path)
{
    m_lockedPaths.remove(path);
}

void SessionFileRegistry::setPendingChatFile(const QString &path)
{
    m_pendingChatFile = path;
}

QString SessionFileRegistry::takePendingChatFile()
{
    return std::exchange(m_pendingChatFile, QString{});
}

QString SessionFileRegistry::uniqueFreePath(const QString &desiredPath) const
{
    if (desiredPath.isEmpty() || !m_lockedPaths.contains(desiredPath)) {
        return desiredPath;
    }

    const QFileInfo info(desiredPath);
    const QString dir = info.path();
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    for (int counter = 2;; ++counter) {
        QString candidate = dir + '/' + base + '_' + QString::number(counter);
        if (!suffix.isEmpty()) {
            candidate += '.' + suffix;
        }
        if (!m_lockedPaths.contains(candidate)) {
            return candidate;
        }
    }
}

} // namespace QodeAssist::Chat
