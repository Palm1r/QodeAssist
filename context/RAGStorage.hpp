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

// RAGStorage.hpp
#pragma once

#include <optional>
#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <qsqlquery.h>

#include <RAGData.hpp>

namespace QodeAssist::Context {

struct FileChunkData
{
    QString filePath;
    int startLine;
    int endLine;
    QString content;
    QDateTime createdAt;
    QDateTime updatedAt;

    bool isValid() const
    {
        return !filePath.isEmpty() && startLine >= 0 && endLine >= startLine && !content.isEmpty();
    }
};

struct StorageOptions
{
    int maxChunkSize = 1024 * 1024;
    int maxVectorSize = 1024;
    bool useCompression = false;
    bool enableLogging = false;
};

struct StorageStatistics
{
    int totalChunks;
    int totalVectors;
    int totalFiles;
    qint64 totalSize;
    QDateTime lastUpdate;
};

class RAGStorage : public QObject
{
    Q_OBJECT

public:
    static constexpr int CURRENT_VERSION = 1;

    enum class Status { Ok, DatabaseError, ValidationError, VersionError, ConnectionError };

    struct ValidationResult
    {
        bool isValid;
        QString errorMessage;
        Status errorStatus;
    };

    struct Error
    {
        QString message;
        QString sqlError;
        QString query;
        Status status;
    };

    explicit RAGStorage(
        const QString &dbPath,
        const StorageOptions &options = StorageOptions(),
        QObject *parent = nullptr);
    ~RAGStorage();

    bool init();
    Status status() const;
    Error lastError() const;
    bool isReady() const;
    QString dbPath() const;

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    bool storeVector(const QString &filePath, const RAGVector &vector);
    bool updateVector(const QString &filePath, const RAGVector &vector);
    std::optional<RAGVector> getVector(const QString &filePath);
    bool needsUpdate(const QString &filePath);
    QStringList getAllFiles();

    bool storeChunk(const FileChunkData &chunk);
    bool storeChunks(const QList<FileChunkData> &chunks);
    bool updateChunk(const FileChunkData &chunk);
    bool updateChunks(const QList<FileChunkData> &chunks);
    bool deleteChunksForFile(const QString &filePath);
    std::optional<FileChunkData> getChunk(const QString &filePath, int startLine, int endLine);
    QList<FileChunkData> getChunksForFile(const QString &filePath);
    bool chunkExists(const QString &filePath, int startLine, int endLine);

    int getChunkCount(const QString &filePath);
    bool deleteOldChunks(const QString &filePath, const QDateTime &olderThan);
    bool deleteAllChunks();
    QStringList getFilesWithChunks();
    bool vacuum();
    bool backup(const QString &backupPath);
    bool restore(const QString &backupPath);
    StorageStatistics getStatistics() const;

    int getStorageVersion() const;
    bool isVersionCompatible() const;

    bool applyMigration(int version);
signals:
    void errorOccurred(const Error &error);
    void operationCompleted(const QString &operation);

private:
    bool createTables();
    bool createIndices();
    bool createVersionTable();
    bool createChunksTable();
    bool createVectorsTable();
    bool openDatabase();
    bool initializeNewStorage();
    bool upgradeStorage(int fromVersion);
    bool validateSchema() const;

    QDateTime getFileLastModified(const QString &filePath);
    RAGVector blobToVector(const QByteArray &blob);
    QByteArray vectorToBlob(const RAGVector &vector);

    void setError(const QString &message, Status status = Status::DatabaseError);
    void clearError();
    bool prepareStatements();
    ValidationResult validateChunk(const FileChunkData &chunk) const;
    ValidationResult validateVector(const RAGVector &vector) const;

private:
    QSqlDatabase m_db;
    QString m_dbPath;
    StorageOptions m_options;
    mutable QMutex m_mutex;
    Error m_lastError;
    Status m_status;

    QSqlQuery m_insertChunkQuery;
    QSqlQuery m_updateChunkQuery;
    QSqlQuery m_insertVectorQuery;
    QSqlQuery m_updateVectorQuery;
};

} // namespace QodeAssist::Context
