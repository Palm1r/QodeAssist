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

#include "RAGStorage.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace QodeAssist::Context {

RAGStorage::RAGStorage(const QString &dbPath, QObject *parent)
    : QObject(parent)
    , m_dbPath(dbPath)
{}

RAGStorage::~RAGStorage()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool RAGStorage::init()
{
    if (!openDatabase()) {
        return false;
    }
    return createTables();
}

bool RAGStorage::openDatabase()
{
    QDir dir(QFileInfo(m_dbPath).absolutePath());
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "rag_storage");
    m_db.setDatabaseName(m_dbPath);

    return m_db.open();
}

bool RAGStorage::createTables()
{
    QSqlQuery query(m_db);
    return query.exec("CREATE TABLE IF NOT EXISTS file_vectors ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "file_path TEXT UNIQUE NOT NULL,"
                      "vector_data BLOB NOT NULL,"
                      "last_modified DATETIME NOT NULL,"
                      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                      "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
                      ")");
}

bool RAGStorage::storeVector(const QString &filePath, const RAGVector &vector)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO file_vectors (file_path, vector_data, last_modified) "
                  "VALUES (:path, :vector, :modified)");

    query.bindValue(":path", filePath);
    query.bindValue(":vector", vectorToBlob(vector));
    query.bindValue(":modified", getFileLastModified(filePath));

    return query.exec();
}

bool RAGStorage::updateVector(const QString &filePath, const RAGVector &vector)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE file_vectors "
                  "SET vector_data = :vector, last_modified = :modified, "
                  "updated_at = CURRENT_TIMESTAMP "
                  "WHERE file_path = :path");

    query.bindValue(":vector", vectorToBlob(vector));
    query.bindValue(":modified", getFileLastModified(filePath));
    query.bindValue(":path", filePath);

    return query.exec();
}

std::optional<RAGVector> RAGStorage::getVector(const QString &filePath)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT vector_data FROM file_vectors WHERE file_path = :path");
    query.bindValue(":path", filePath);

    if (query.exec() && query.next()) {
        return blobToVector(query.value(0).toByteArray());
    }
    return std::nullopt;
}

bool RAGStorage::needsUpdate(const QString &filePath)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT last_modified FROM file_vectors WHERE file_path = :path");
    query.bindValue(":path", filePath);

    if (query.exec() && query.next()) {
        QDateTime storedTime = query.value(0).toDateTime();
        return storedTime < getFileLastModified(filePath);
    }
    return true;
}

QStringList RAGStorage::getAllFiles()
{
    QStringList files;
    QSqlQuery query(m_db);

    if (query.exec("SELECT file_path FROM file_vectors")) {
        while (query.next()) {
            files << query.value(0).toString();
        }
    }
    return files;
}

QDateTime RAGStorage::getFileLastModified(const QString &filePath)
{
    return QFileInfo(filePath).lastModified();
}

RAGVector RAGStorage::blobToVector(const QByteArray &blob)
{
    RAGVector vector;
    const float *data = reinterpret_cast<const float *>(blob.constData());
    size_t size = blob.size() / sizeof(float);
    vector.assign(data, data + size);
    return vector;
}

QByteArray RAGStorage::vectorToBlob(const RAGVector &vector)
{
    return QByteArray(reinterpret_cast<const char *>(vector.data()), vector.size() * sizeof(float));
}

QString RAGStorage::dbPath() const
{
    return m_dbPath;
}

} // namespace QodeAssist::Context
