// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FileItem.hpp"

#include <QDesktopServices>
#include <QUrl>

#include <coreplugin/editormanager/editormanager.h>
#include <logger/Logger.hpp>
#include <utils/filepath.h>

namespace QodeAssist::Chat {

FileItem::FileItem(QQuickItem *parent)
    : QQuickItem(parent)
{}

void FileItem::openFileInEditor()
{
    if (m_filePath.isEmpty()) {
        return;
    }

    Utils::FilePath filePathObj = Utils::FilePath::fromString(m_filePath);
    Core::IEditor *editor = Core::EditorManager::openEditor(filePathObj);

    if (!editor) {
        LOG_MESSAGE(QString("Failed to open file in editor: %1").arg(m_filePath));
    }
}

void FileItem::openFileInExternalEditor()
{
    if (m_filePath.isEmpty()) {
        return;
    }

    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(m_filePath));
    if (success) {
        LOG_MESSAGE(QString("Opened file in external application: %1").arg(m_filePath));
    } else {
        LOG_MESSAGE(QString("Failed to open file externally: %1").arg(m_filePath));
    }
}

QString FileItem::filePath() const
{
    return m_filePath;
}

void FileItem::setFilePath(const QString &newFilePath)
{
    if (m_filePath == newFilePath)
        return;
    m_filePath = newFilePath;
    emit filePathChanged();
}

} // namespace QodeAssist::Chat
