/*
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "FileItem.hpp"

#include <utils/filepath.h>
#include <coreplugin/editormanager/editormanager.h>
#include <logger/Logger.hpp>

namespace QodeAssist::Chat {

FileItem::FileItem(QQuickItem *parent)
    : QQuickItem(parent)
{}

void FileItem::openFileInEditor() {
    if (m_filePath.isEmpty()) {
        return;
    }

    Utils::FilePath filePathObj = Utils::FilePath::fromString(m_filePath);
    Core::IEditor *editor = Core::EditorManager::openEditor(filePathObj);

    if (!editor) {
        LOG_MESSAGE(QString("Failed to open file in editor: %1").arg(m_filePath));
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
