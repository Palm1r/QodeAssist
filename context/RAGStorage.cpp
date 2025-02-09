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

// RAGStorage.cpp
#include "RAGStorage.hpp"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace QodeAssist::Context {

RAGStorage::RAGStorage(const QString &dbPath, const StorageOptions &options, QObject *parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_options(options)
    , m_status(Status::Ok)
{}

RAGStorage::~RAGStorage()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(m_db.connectionName());
}

bool RAGStorage::init()
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "Initializing RAGStorage at path:" << m_dbPath;

    if (!openDatabase()) {
        qDebug() << "Failed to open database";
        return false;
    }
    qDebug() << "Database opened successfully";

    if (!createTables()) {
        qDebug() << "Failed to create tables";
        return false;
    }
    qDebug() << "Tables created successfully";

    if (!createIndices()) {
        qDebug() << "Failed to create indices";
        return false;
    }
    qDebug() << "Indices created successfully";

    int version = getStorageVersion();
    qDebug() << "Current storage version:" << version;

    if (version < CURRENT_VERSION) {
        qDebug() << "Upgrading storage from version" << version << "to" << CURRENT_VERSION;
        if (!upgradeStorage(version)) {
            qDebug() << "Failed to upgrade storage";
            return false;
        }
        qDebug() << "Storage upgraded successfully";
    }

    if (!prepareStatements()) {
        qDebug() << "Failed to prepare statements";
        return false;
    }
    qDebug() << "Statements prepared successfully";

    m_status = Status::Ok;
    qDebug() << "RAGStorage initialized successfully";
    return true;
}

bool RAGStorage::openDatabase()
{
    qDebug() << "Opening database at:" << m_dbPath;

    QDir dir(QFileInfo(m_dbPath).absolutePath());
    if (!dir.exists() && !dir.mkpath(".")) {
        setError("Failed to create database directory", Status::DatabaseError);
        return false;
    }

    QString connectionName = QString("rag_storage_%1").arg(QUuid::createUuid().toString());
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        setError("Failed to open database: " + m_db.lastError().text(), Status::ConnectionError);
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec("PRAGMA journal_mode=WAL")) {
        qDebug() << "Failed to set journal mode:" << query.lastError().text();
    }

    if (!query.exec("PRAGMA synchronous=NORMAL")) {
        qDebug() << "Failed to set synchronous mode:" << query.lastError().text();
    }

    qDebug() << "Database opened successfully";
    return true;
}

bool RAGStorage::createTables()
{
    qDebug() << "Creating tables...";

    if (!createVersionTable()) {
        qDebug() << "Failed to create version table";
        return false;
    }
    qDebug() << "Version table created";

    if (!createVectorsTable()) {
        qDebug() << "Failed to create vectors table";
        return false;
    }
    qDebug() << "Vectors table created";

    if (!createChunksTable()) {
        qDebug() << "Failed to create chunks table";
        return false;
    }
    qDebug() << "Chunks table created";

    return true;
}

bool RAGStorage::createIndices()
{
    QSqlQuery query(m_db);

    QStringList indices
        = {"CREATE INDEX IF NOT EXISTS idx_file_chunks_file_path ON file_chunks(file_path)",
           "CREATE INDEX IF NOT EXISTS idx_file_vectors_file_path ON file_vectors(file_path)",
           "CREATE INDEX IF NOT EXISTS idx_file_chunks_updated_at ON file_chunks(updated_at)"};

    for (const QString &sql : indices) {
        if (!query.exec(sql)) {
            setError("Failed to create index: " + query.lastError().text());
            return false;
        }
    }
    return true;
}

bool RAGStorage::createVersionTable()
{
    qDebug() << "Creating version table...";

    QSqlQuery query(m_db);
    bool success = query.exec("CREATE TABLE IF NOT EXISTS storage_version ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "version INTEGER NOT NULL,"
                              "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
                              ")");

    if (!success) {
        qDebug() << "Failed to create version table:" << query.lastError().text();
        return false;
    }

    query.exec("SELECT COUNT(*) FROM storage_version");
    if (query.next() && query.value(0).toInt() == 0) {
        qDebug() << "Inserting initial version record";
        QSqlQuery insertQuery(m_db);
        success = insertQuery.exec(
            QString("INSERT INTO storage_version (version) VALUES (%1)").arg(CURRENT_VERSION));
        if (!success) {
            qDebug() << "Failed to insert initial version:" << insertQuery.lastError().text();
            return false;
        }
    }

    qDebug() << "Version table ready";
    return true;
}

bool RAGStorage::createVectorsTable()
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

bool RAGStorage::createChunksTable()
{
    QSqlQuery query(m_db);
    return query.exec("CREATE TABLE IF NOT EXISTS file_chunks ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "file_path TEXT NOT NULL,"
                      "start_line INTEGER NOT NULL CHECK (start_line >= 0),"
                      "end_line INTEGER NOT NULL CHECK (end_line >= start_line),"
                      "content TEXT NOT NULL,"
                      "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                      "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                      "UNIQUE(file_path, start_line, end_line)"
                      ")");
}

bool RAGStorage::prepareStatements()
{
    qDebug() << "Preparing SQL statements...";

    m_insertChunkQuery = QSqlQuery(m_db);
    if (!m_insertChunkQuery.prepare(
            "INSERT INTO file_chunks (file_path, start_line, end_line, content) "
            "VALUES (:path, :start, :end, :content)")) {
        setError("Failed to prepare insert chunk query");
        return false;
    }

    m_updateChunkQuery = QSqlQuery(m_db);
    if (!m_updateChunkQuery.prepare(
            "UPDATE file_chunks SET content = :content, updated_at = CURRENT_TIMESTAMP "
            "WHERE file_path = :path AND start_line = :start AND end_line = :end")) {
        setError("Failed to prepare update chunk query");
        return false;
    }

    m_insertVectorQuery = QSqlQuery(m_db);
    if (!m_insertVectorQuery.prepare(
            "INSERT INTO file_vectors (file_path, vector_data, last_modified) "
            "VALUES (:path, :vector, :modified)")) {
        setError("Failed to prepare insert vector query: " + m_insertVectorQuery.lastError().text());
        return false;
    }

    m_updateVectorQuery = QSqlQuery(m_db);
    if (!m_updateVectorQuery.prepare(
            "UPDATE file_vectors SET vector_data = :vector, last_modified = :modified, "
            "updated_at = CURRENT_TIMESTAMP WHERE file_path = :path")) {
        setError("Failed to prepare update vector query: " + m_updateVectorQuery.lastError().text());
        return false;
    }

    return true;
}

bool RAGStorage::storeChunk(const FileChunkData &chunk)
{
    QMutexLocker locker(&m_mutex);

    auto validation = validateChunk(chunk);
    if (!validation.isValid) {
        setError(validation.errorMessage, validation.errorStatus);
        return false;
    }

    if (!beginTransaction()) {
        return false;
    }

    m_insertChunkQuery.bindValue(":path", chunk.filePath);
    m_insertChunkQuery.bindValue(":start", chunk.startLine);
    m_insertChunkQuery.bindValue(":end", chunk.endLine);
    m_insertChunkQuery.bindValue(":content", chunk.content);

    if (!m_insertChunkQuery.exec()) {
        rollbackTransaction();
        setError("Failed to store chunk: " + m_insertChunkQuery.lastError().text());
        return false;
    }

    return commitTransaction();
}

bool RAGStorage::storeChunks(const QList<FileChunkData> &chunks)
{
    QMutexLocker locker(&m_mutex);

    if (!beginTransaction()) {
        return false;
    }

    for (const auto &chunk : chunks) {
        auto validation = validateChunk(chunk);
        if (!validation.isValid) {
            setError(validation.errorMessage, validation.errorStatus);
            rollbackTransaction();
            return false;
        }

        m_insertChunkQuery.bindValue(":path", chunk.filePath);
        m_insertChunkQuery.bindValue(":start", chunk.startLine);
        m_insertChunkQuery.bindValue(":end", chunk.endLine);
        m_insertChunkQuery.bindValue(":content", chunk.content);

        if (!m_insertChunkQuery.exec()) {
            rollbackTransaction();
            setError("Failed to store chunks: " + m_insertChunkQuery.lastError().text());
            return false;
        }
    }

    return commitTransaction();
}

RAGStorage::ValidationResult RAGStorage::validateChunk(const FileChunkData &chunk) const
{
    if (!chunk.isValid()) {
        return {false, "Invalid chunk data", Status::ValidationError};
    }

    if (chunk.content.size() > m_options.maxChunkSize) {
        return {false, "Chunk content exceeds maximum size", Status::ValidationError};
    }

    return {true, QString(), Status::Ok};
}

RAGStorage::ValidationResult RAGStorage::validateVector(const RAGVector &vector) const
{
    if (vector.empty()) {
        return {false, "Empty vector data", Status::ValidationError};
    }

    if (vector.size() > m_options.maxVectorSize) {
        return {false, "Vector size exceeds maximum limit", Status::ValidationError};
    }

    return {true, QString(), Status::Ok};
}

bool RAGStorage::beginTransaction()
{
    return m_db.transaction();
}

bool RAGStorage::commitTransaction()
{
    return m_db.commit();
}

bool RAGStorage::rollbackTransaction()
{
    return m_db.rollback();
}

bool RAGStorage::storeVector(const QString &filePath, const RAGVector &vector)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "Storing vector for file:" << filePath;

    auto validation = validateVector(vector);
    if (!validation.isValid) {
        setError(validation.errorMessage, validation.errorStatus);
        return false;
    }

    if (!beginTransaction()) {
        return false;
    }

    QDateTime lastModified = getFileLastModified(filePath);
    QByteArray blob = vectorToBlob(vector);
    qDebug() << "Vector converted to blob, size:" << blob.size() << "bytes";

    m_updateVectorQuery.bindValue(":path", filePath);
    m_updateVectorQuery.bindValue(":vector", blob);
    m_updateVectorQuery.bindValue(":modified", lastModified);

    if (m_updateVectorQuery.exec()) {
        if (m_updateVectorQuery.numRowsAffected() > 0) {
            qDebug() << "Vector updated successfully";
            return commitTransaction();
        }
    }

    m_insertVectorQuery.bindValue(":path", filePath);
    m_insertVectorQuery.bindValue(":vector", blob);
    m_insertVectorQuery.bindValue(":modified", lastModified);

    if (!m_insertVectorQuery.exec()) {
        qDebug() << "Failed to store vector:" << m_insertVectorQuery.lastError().text();
        rollbackTransaction();
        setError("Failed to store vector: " + m_insertVectorQuery.lastError().text());
        return false;
    }

    qDebug() << "Vector stored successfully";
    return commitTransaction();
}

bool RAGStorage::updateVector(const QString &filePath, const RAGVector &vector)
{
    QMutexLocker locker(&m_mutex);

    auto validation = validateVector(vector);
    if (!validation.isValid) {
        setError(validation.errorMessage, validation.errorStatus);
        return false;
    }

    if (!beginTransaction()) {
        return false;
    }

    QDateTime lastModified = getFileLastModified(filePath);
    QByteArray blob = vectorToBlob(vector);

    m_updateVectorQuery.bindValue(":path", filePath);
    m_updateVectorQuery.bindValue(":vector", blob);
    m_updateVectorQuery.bindValue(":modified", lastModified);

    if (!m_updateVectorQuery.exec()) {
        rollbackTransaction();
        setError("Failed to update vector: " + m_updateVectorQuery.lastError().text());
        return false;
    }

    return commitTransaction();
}

std::optional<RAGVector> RAGStorage::getVector(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT vector_data FROM file_vectors WHERE file_path = :path");
    query.bindValue(":path", filePath);

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    QByteArray blob = query.value(0).toByteArray();
    return blobToVector(blob);
}

bool RAGStorage::needsUpdate(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT last_modified FROM file_vectors WHERE file_path = :path");
    query.bindValue(":path", filePath);

    if (!query.exec() || !query.next()) {
        return true;
    }

    QDateTime storedModified = query.value(0).toDateTime();
    QDateTime fileModified = getFileLastModified(filePath);

    return fileModified > storedModified;
}

QDateTime RAGStorage::getFileLastModified(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.lastModified();
}

RAGVector RAGStorage::blobToVector(const QByteArray &blob)
{
    RAGVector vector;
    QDataStream stream(blob);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);

    qint32 size;
    stream >> size;

    vector.resize(size);
    for (int i = 0; i < size; ++i) {
        double value;
        stream >> value;
        vector[i] = value;
    }

    qDebug() << "Vector restored from blob, size:" << vector.size();

    return vector;
}

QByteArray RAGStorage::vectorToBlob(const RAGVector &vector)
{
    QByteArray blob;
    QDataStream stream(&blob, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);

    stream << static_cast<qint32>(vector.size());

    for (double value : vector) {
        stream << value;
    }

    qDebug() << "Vector converted to blob, vector size:" << vector.size()
             << "blob size:" << blob.size();

    return blob;
}

void RAGStorage::setError(const QString &message, Status status)
{
    m_lastError = Error{message, m_db.lastError().text(), m_db.lastError().databaseText(), status};
    m_status = status;
    emit errorOccurred(m_lastError);
}

void RAGStorage::clearError()
{
    m_lastError = Error();
    m_status = Status::Ok;
}

RAGStorage::Status RAGStorage::status() const
{
    return m_status;
}

RAGStorage::Error RAGStorage::lastError() const
{
    return m_lastError;
}

bool RAGStorage::isReady() const
{
    return m_db.isOpen() && m_status == Status::Ok;
}

QString RAGStorage::dbPath() const
{
    return m_dbPath;
}

bool RAGStorage::deleteChunksForFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    if (!beginTransaction()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM file_chunks WHERE file_path = :path");
    query.bindValue(":path", filePath);

    if (!query.exec()) {
        rollbackTransaction();
        setError("Failed to delete chunks: " + query.lastError().text());
        return false;
    }

    return commitTransaction();
}

bool RAGStorage::vacuum()
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    if (!query.exec("VACUUM")) {
        setError("Failed to vacuum database: " + query.lastError().text());
        return false;
    }
    return true;
}

bool RAGStorage::backup(const QString &backupPath)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        setError("Database is not open");
        return false;
    }

    QFile::copy(m_dbPath, backupPath);

    return true;
}

StorageStatistics RAGStorage::getStatistics() const
{
    QMutexLocker locker(&m_mutex);

    StorageStatistics stats;
    QSqlQuery query(m_db);

    if (query.exec("SELECT COUNT(*), SUM(LENGTH(content)) FROM file_chunks")) {
        if (query.next()) {
            stats.totalChunks = query.value(0).toInt();
            stats.totalSize = query.value(1).toLongLong();
        }
    }

    if (query.exec("SELECT COUNT(*) FROM file_vectors")) {
        if (query.next()) {
            stats.totalVectors = query.value(0).toInt();
        }
    }
    if (query.exec("SELECT COUNT(DISTINCT file_path) FROM ("
                   "SELECT file_path FROM file_chunks "
                   "UNION "
                   "SELECT file_path FROM file_vectors)")) {
        if (query.next()) {
            stats.totalFiles = query.value(0).toInt();
        }
    }

    if (query.exec("SELECT MAX(updated_at) FROM ("
                   "SELECT updated_at FROM file_chunks "
                   "UNION "
                   "SELECT updated_at FROM file_vectors)")) {
        if (query.next()) {
            stats.lastUpdate = query.value(0).toDateTime();
        }
    }

    return stats;
}

bool RAGStorage::deleteOldChunks(const QString &filePath, const QDateTime &olderThan)
{
    QMutexLocker locker(&m_mutex);

    if (!beginTransaction()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM file_chunks WHERE file_path = :path AND updated_at < :date");
    query.bindValue(":path", filePath);
    query.bindValue(":date", olderThan);

    if (!query.exec()) {
        rollbackTransaction();
        setError("Failed to delete old chunks: " + query.lastError().text());
        return false;
    }

    return commitTransaction();
}

bool RAGStorage::deleteAllChunks()
{
    QMutexLocker locker(&m_mutex);

    if (!beginTransaction()) {
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM file_chunks")) {
        rollbackTransaction();
        setError("Failed to delete all chunks: " + query.lastError().text());
        return false;
    }

    return commitTransaction();
}

QStringList RAGStorage::getFilesWithChunks()
{
    QMutexLocker locker(&m_mutex);

    QStringList files;
    QSqlQuery query(m_db);

    if (query.exec("SELECT DISTINCT file_path FROM file_chunks")) {
        while (query.next()) {
            files.append(query.value(0).toString());
        }
    }

    return files;
}

QStringList RAGStorage::getAllFiles()
{
    QMutexLocker locker(&m_mutex);

    QStringList files;
    QSqlQuery query(m_db);

    if (query.exec("SELECT DISTINCT file_path FROM ("
                   "SELECT file_path FROM file_chunks "
                   "UNION "
                   "SELECT file_path FROM file_vectors)")) {
        while (query.next()) {
            files.append(query.value(0).toString());
        }
    }

    return files;
}

bool RAGStorage::updateChunk(const FileChunkData &chunk)
{
    QMutexLocker locker(&m_mutex);

    auto validation = validateChunk(chunk);
    if (!validation.isValid) {
        return false;
    }

    if (!beginTransaction()) {
        return false;
    }

    m_updateChunkQuery.bindValue(":path", chunk.filePath);
    m_updateChunkQuery.bindValue(":start", chunk.startLine);
    m_updateChunkQuery.bindValue(":end", chunk.endLine);
    m_updateChunkQuery.bindValue(":content", chunk.content);

    if (!m_updateChunkQuery.exec()) {
        rollbackTransaction();
        setError("Failed to update chunk: " + m_updateChunkQuery.lastError().text());
        return false;
    }

    return commitTransaction();
}

bool RAGStorage::updateChunks(const QList<FileChunkData> &chunks)
{
    QMutexLocker locker(&m_mutex);

    if (!beginTransaction()) {
        return false;
    }

    for (const auto &chunk : chunks) {
        auto validation = validateChunk(chunk);
        if (!validation.isValid) {
            rollbackTransaction();
            return false;
        }

        m_updateChunkQuery.bindValue(":path", chunk.filePath);
        m_updateChunkQuery.bindValue(":start", chunk.startLine);
        m_updateChunkQuery.bindValue(":end", chunk.endLine);
        m_updateChunkQuery.bindValue(":content", chunk.content);

        if (!m_updateChunkQuery.exec()) {
            rollbackTransaction();
            setError("Failed to update chunks: " + m_updateChunkQuery.lastError().text());
            return false;
        }
    }

    return commitTransaction();
}

std::optional<FileChunkData> RAGStorage::getChunk(const QString &filePath, int startLine, int endLine)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT file_path, start_line, end_line, content, created_at, updated_at "
                  "FROM file_chunks "
                  "WHERE file_path = :path AND start_line = :start AND end_line = :end");

    query.bindValue(":path", filePath);
    query.bindValue(":start", startLine);
    query.bindValue(":end", endLine);

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    FileChunkData chunk;
    chunk.filePath = query.value(0).toString();
    chunk.startLine = query.value(1).toInt();
    chunk.endLine = query.value(2).toInt();
    chunk.content = query.value(3).toString();
    chunk.createdAt = query.value(4).toDateTime();
    chunk.updatedAt = query.value(5).toDateTime();

    return chunk;
}

QList<FileChunkData> RAGStorage::getChunksForFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    QList<FileChunkData> chunks;
    QSqlQuery query(m_db);
    query.prepare("SELECT file_path, start_line, end_line, content, created_at, updated_at "
                  "FROM file_chunks "
                  "WHERE file_path = :path "
                  "ORDER BY start_line");

    query.bindValue(":path", filePath);

    if (query.exec()) {
        while (query.next()) {
            FileChunkData chunk;
            chunk.filePath = query.value(0).toString();
            chunk.startLine = query.value(1).toInt();
            chunk.endLine = query.value(2).toInt();
            chunk.content = query.value(3).toString();
            chunk.createdAt = query.value(4).toDateTime();
            chunk.updatedAt = query.value(5).toDateTime();
            chunks.append(chunk);
        }
    }

    return chunks;
}

bool RAGStorage::chunkExists(const QString &filePath, int startLine, int endLine)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM file_chunks "
                  "WHERE file_path = :path AND start_line = :start AND end_line = :end");

    query.bindValue(":path", filePath);
    query.bindValue(":start", startLine);
    query.bindValue(":end", endLine);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

int RAGStorage::getStorageVersion() const
{
    qDebug() << "Getting storage version...";

    QSqlQuery query(m_db);
    qDebug() << "Created query object";

    if (!query.exec("SELECT version FROM storage_version ORDER BY id DESC LIMIT 1")) {
        qDebug() << "Failed to execute version query:" << query.lastError().text();
        return 0;
    }
    qDebug() << "Version query executed";

    if (query.next()) {
        int version = query.value(0).toInt();
        qDebug() << "Current version:" << version;
        return version;
    }

    qDebug() << "No version found, assuming version 0";
    return 0;
}

bool RAGStorage::isVersionCompatible() const
{
    int version = getStorageVersion();
    return version > 0 && version <= CURRENT_VERSION;
}

bool RAGStorage::upgradeStorage(int fromVersion)
{
    QMutexLocker locker(&m_mutex);

    if (!beginTransaction()) {
        return false;
    }

    for (int version = fromVersion + 1; version <= CURRENT_VERSION; ++version) {
        if (!applyMigration(version)) {
            rollbackTransaction();
            setError(QString("Failed to upgrade to version %1").arg(version));
            return false;
        }
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO storage_version (version) VALUES (:version)");
    query.bindValue(":version", CURRENT_VERSION);

    if (!query.exec()) {
        rollbackTransaction();
        setError("Failed to update storage version");
        return false;
    }

    return commitTransaction();
}

bool RAGStorage::applyMigration(int version)
{
    QSqlQuery query(m_db);

    switch (version) {
    case 1:
        if (!query.exec("ALTER TABLE file_chunks ADD COLUMN metadata TEXT")) {
            return false;
        }
        break;

        // case 2:
        //     //
        //     break;

    default:
        setError(QString("Unknown version for migration: %1").arg(version));
        return false;
    }

    return true;
}

bool RAGStorage::validateSchema() const
{
    QMutexLocker locker(&m_mutex);

    QStringList requiredTables = {"storage_version", "file_vectors", "file_chunks"};

    QSqlQuery query(m_db);
    query.exec("SELECT name FROM sqlite_master WHERE type='table'");

    QStringList existingTables;
    while (query.next()) {
        existingTables << query.value(0).toString();
    }

    for (const QString &table : requiredTables) {
        if (!existingTables.contains(table)) {
            return false;
        }
    }

    struct ColumnInfo
    {
        QString name;
        QString type;
        bool notNull;
    };

    QMap<QString, QList<ColumnInfo>> expectedSchema
        = {{"file_chunks",
            {{"id", "INTEGER", true},
             {"file_path", "TEXT", true},
             {"start_line", "INTEGER", true},
             {"end_line", "INTEGER", true},
             {"content", "TEXT", true},
             {"created_at", "DATETIME", false},
             {"updated_at", "DATETIME", false}}},
           {"file_vectors",
            {{"id", "INTEGER", true},
             {"file_path", "TEXT", true},
             {"vector_data", "BLOB", true},
             {"last_modified", "DATETIME", true},
             {"created_at", "DATETIME", false},
             {"updated_at", "DATETIME", false}}}};

    for (auto it = expectedSchema.begin(); it != expectedSchema.end(); ++it) {
        QString tableName = it.key();
        query.exec(QString("PRAGMA table_info(%1)").arg(tableName));

        QList<ColumnInfo> actualColumns;
        while (query.next()) {
            ColumnInfo col;
            col.name = query.value(1).toString();
            col.type = query.value(2).toString();
            col.notNull = query.value(3).toBool();
            actualColumns << col;
        }

        if (actualColumns.size() != it.value().size()) {
            return false;
        }

        for (int i = 0; i < actualColumns.size(); ++i) {
            const auto &expected = it.value()[i];
            const auto &actual = actualColumns[i];

            if (expected.name != actual.name || expected.type != actual.type
                || expected.notNull != actual.notNull) {
                return false;
            }
        }
    }

    return true;
}

bool RAGStorage::restore(const QString &backupPath)
{
    QMutexLocker locker(&m_mutex);

    if (m_db.isOpen()) {
        m_db.close();
    }

    if (!QFile::remove(m_dbPath) || !QFile::copy(backupPath, m_dbPath)) {
        setError("Failed to restore from backup");
        return false;
    }

    if (!openDatabase()) {
        return false;
    }

    if (!validateSchema()) {
        setError("Invalid schema in backup file");
        return false;
    }

    return true;
}

int RAGStorage::getChunkCount(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM file_chunks WHERE file_path = :path");
    query.bindValue(":path", filePath);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

} // namespace QodeAssist::Context
