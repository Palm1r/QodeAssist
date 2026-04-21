// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

