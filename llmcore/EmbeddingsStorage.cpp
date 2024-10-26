#include "EmbeddingsStorage.hpp"
#include "Logger.hpp"

#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtCore/qcryptographichash.h>

namespace QodeAssist::LLMCore {

EmbeddingsStorage &EmbeddingsStorage::instance()
{
    static EmbeddingsStorage instance;
    return instance;
}

EmbeddingsStorage::EmbeddingsStorage(QObject *parent)
    : QObject(parent)
{
    QString basePath = QString("/Users/palm1r/Projects/TestEmbeding") + "/embeddings";

    m_storageDir.setPath(basePath);
    m_vectorsDir.setPath(basePath + "/vectors");
    m_metadataDir.setPath(basePath + "/metadata");

    ensureDirectoriesExist();
}

void EmbeddingsStorage::ensureDirectoriesExist() const
{
    if (!m_storageDir.exists()) {
        m_storageDir.mkpath(".");
    }
    if (!m_vectorsDir.exists()) {
        m_vectorsDir.mkpath(".");
    }
    if (!m_metadataDir.exists()) {
        m_metadataDir.mkpath(".");
    }
}

QString EmbeddingsStorage::generateFileName(const CodeChunk &chunk,
                                            const QVector<float> &embedding) const
{
    // Создаем хеш из содержимого чанка
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(chunk.content.toUtf8());
    QString vectorHash = hash.result().toHex().left(8);

    // Формируем имя: hash_base_ext_line.{vec|json}
    // Например: a1b2c3d4_main_cpp_42
    QFileInfo info(chunk.filePath);
    return QString("%1_%2_%3_%4")
        .arg(vectorHash)
        .arg(info.baseName())
        .arg(info.suffix())
        .arg(chunk.startLine);
}

QString EmbeddingsStorage::generateStoragePath(const QString &fileName, bool isMetadata) const
{
    if (isMetadata) {
        return m_metadataDir.filePath(fileName + ".json");
    }
    return m_vectorsDir.filePath(fileName + ".vec");
}

bool EmbeddingsStorage::storeEmbedding(const CodeChunk &chunk, const QVector<float> &embedding)
{
    QString fileName = generateFileName(chunk, embedding);

    // Сохраняем метаданные
    QJsonObject metadata;
    metadata["fileName"] = fileName;
    metadata["filePath"] = chunk.filePath;
    metadata["startLine"] = chunk.startLine;
    metadata["endLine"] = chunk.endLine;
    metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["content"] = chunk.content;
    metadata["vectorHash"] = fileName.split('_').first(); // Сохраняем хеш для удобства

    if (!chunk.overlapContent.isEmpty()) {
        metadata["overlapContent"] = chunk.overlapContent;
    }

    QFile metaFile(generateStoragePath(fileName, true));
    if (!metaFile.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open metadata file for writing:" << metaFile.fileName();
        return false;
    }

    metaFile.write(QJsonDocument(metadata).toJson());
    metaFile.close();

    // Сохраняем вектор
    QFile vecFile(generateStoragePath(fileName, false));
    if (!vecFile.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open vector file for writing:" << vecFile.fileName();
        metaFile.remove();
        return false;
    }

    QDataStream stream(&vecFile);
    stream << embedding;
    vecFile.close();

    qDebug() << "Stored embedding for chunk from file:" << chunk.filePath
             << "lines:" << chunk.startLine << "-" << chunk.endLine;

    return true;
}

QVector<EmbeddingsStorage::SearchResult> EmbeddingsStorage::findSimilarCode(
    const QVector<float> &queryEmbedding, float minSimilarity, int maxResults)
{
    QVector<SearchResult> results;
    QVector<QPair<float, QString>> candidates;

    // Читаем все векторы, имя файла уже содержит хеш
    QDirIterator vecIt(m_vectorsDir.path(), QStringList() << "*.vec", QDir::Files);
    while (vecIt.hasNext()) {
        QString vecPath = vecIt.next();
        QString fileName = QFileInfo(vecPath).baseName(); // Включает хеш

        QFile vecFile(vecPath);
        if (!vecFile.open(QIODevice::ReadOnly)) {
            continue;
        }

        QVector<float> storedEmbedding;
        QDataStream stream(&vecFile);
        stream >> storedEmbedding;

        if (storedEmbedding.size() == queryEmbedding.size()) {
            float similarity = cosineDistance(queryEmbedding, storedEmbedding);
            if (similarity >= minSimilarity) {
                candidates.append({similarity, fileName});
            }
        }
    }

    // Сортируем по убыванию сходства
    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        return a.first > b.first;
    });

    // Берем top-k результатов
    int count = qMin(maxResults, candidates.size());
    for (int i = 0; i < count; ++i) {
        const auto &[similarity, fileName] = candidates[i];

        // Загружаем метаданные для найденных совпадений
        QFile metaFile(m_metadataDir.filePath(fileName + ".json"));
        if (!metaFile.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
        QJsonObject metadata = doc.object();

        SearchResult result;
        result.content = metadata["content"].toString();
        result.filePath = metadata["filePath"].toString();
        result.startLine = metadata["startLine"].toInt();
        result.endLine = metadata["endLine"].toInt();
        result.similarity = similarity;

        results.append(result);

        qDebug() << "Found similar code in" << result.filePath << "lines" << result.startLine << "-"
                 << result.endLine << "with similarity" << similarity;
    }

    return results;
}

bool EmbeddingsStorage::isFileUpToDate(const QString &filePath) const
{
    QFileInfo sourceFile(filePath);
    if (!sourceFile.exists()) {
        return false;
    }

    QDateTime lastModified = sourceFile.lastModified();
    QDirIterator it(m_metadataDir.path(), QStringList() << "*.json", QDir::Files);

    while (it.hasNext()) {
        QFile metaFile(it.next());
        if (metaFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
            QJsonObject metadata = doc.object();

            if (metadata["filePath"].toString() == filePath) {
                QDateTime storedTime = QDateTime::fromString(metadata["timestamp"].toString(),
                                                             Qt::ISODate);

                if (storedTime < lastModified) {
                    return false;
                }
            }
        }
    }

    return true;
}

void EmbeddingsStorage::clearFileEmbeddings(const QString &filePath)
{
    QFileInfo info(filePath);
    QString filePattern = QString("*_%1_%2_*").arg(info.baseName(), info.suffix());

    QDirIterator it(m_metadataDir.path(), QStringList() << filePattern, QDir::Files);
    while (it.hasNext()) {
        QFile metaFile(it.next());
        QString fileName = QFileInfo(metaFile).baseName();

        // Удаляем оба файла
        QFile::remove(generateStoragePath(fileName, false)); // .vec
        metaFile.remove();                                   // .json

        qDebug() << "Removed embeddings for file:" << fileName;
    }
}

float EmbeddingsStorage::cosineDistance(const QVector<float> &a, const QVector<float> &b)
{
    if (a.size() != b.size() || a.isEmpty()) {
        return 0.0f;
    }

    const float *ptrA = a.constData();
    const float *ptrB = b.constData();
    const int size = a.size();

    float dotProduct = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;

    for (int i = 0; i < size; i++) {
        dotProduct += ptrA[i] * ptrB[i];
        normA += ptrA[i] * ptrA[i];
        normB += ptrB[i] * ptrB[i];
    }

    float denominator = std::sqrt(normA) * std::sqrt(normB);
    if (denominator == 0) {
        return 0.0f;
    }

    return dotProduct / denominator;
}

} // namespace QodeAssist::LLMCore
