/*
 * Copyright (C) 2024 Petr Mironychev
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

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include <RAGData.hpp>

namespace QodeAssist::Context {

class RAGStorage : public QObject
{
    Q_OBJECT
public:
    static constexpr int CURRENT_VERSION = 1;

    explicit RAGStorage(const QString &dbPath, QObject *parent = nullptr);
    ~RAGStorage();

    bool init();
    bool storeVector(const QString &filePath, const RAGVector &vector);
    bool updateVector(const QString &filePath, const RAGVector &vector);
    std::optional<RAGVector> getVector(const QString &filePath);
    bool needsUpdate(const QString &filePath);
    QStringList getAllFiles();
    QString dbPath() const;

    // Новые методы для работы с версией
    int getStorageVersion() const;
    bool isVersionCompatible() const { return getStorageVersion() == CURRENT_VERSION; }

private:
    bool createTables();
    bool createVersionTable();
    bool openDatabase();
    bool initializeNewStorage();
    bool upgradeStorage(int fromVersion);
    QDateTime getFileLastModified(const QString &filePath);
    RAGVector blobToVector(const QByteArray &blob);
    QByteArray vectorToBlob(const RAGVector &vector);

    QSqlDatabase m_db;
    QString m_dbPath;
};

} // namespace QodeAssist::Context
