/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

namespace QodeAssist::Chat {

class ChatFileManager : public QObject
{
    Q_OBJECT

public:
    explicit ChatFileManager(QObject *parent = nullptr);
    ~ChatFileManager();

    QStringList processDroppedFiles(const QStringList &filePaths);
    void setChatFilePath(const QString &chatFilePath);
    QString chatFilePath() const;
    void clearIntermediateStorage();

    static bool isFileAccessible(const QString &filePath);
    static void cleanupGlobalIntermediateStorage();

signals:
    void fileOperationFailed(const QString &error);
    void fileCopiedToStorage(const QString &originalPath, const QString &newPath);

private:
    QString copyToIntermediateStorage(const QString &filePath);
    QString getIntermediateStorageDir();
    QString generateIntermediateFileName(const QString &originalPath);

    QString m_chatFilePath;
    QString m_intermediateStorageDir;
};

} // namespace QodeAssist::Chat

