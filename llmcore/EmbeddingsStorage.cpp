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

QVector<SearchResult> EmbeddingsStorage::findSimilarCode(const QVector<float> &queryEmbedding,
                                                         float minSimilarity,
                                                         int maxResults)
{
    SearchConfig config;
    config.minSimilarity = minSimilarity;
    config.maxResults = maxResults;

    QVector<std::pair<SearchResult, float>> preliminaryResults;
    QVector<float> similarities;

    // Первый проход - собираем все схожести
    QDirIterator vecIt(m_vectorsDir.path(), QStringList() << "*.vec", QDir::Files);
    while (vecIt.hasNext()) {
        QString vecPath = vecIt.next();
        QFile vecFile(vecPath);
        if (!vecFile.open(QIODevice::ReadOnly)) {
            continue;
        }

        QVector<float> storedEmbedding;
        QDataStream stream(&vecFile);
        stream >> storedEmbedding;

        if (storedEmbedding.size() != queryEmbedding.size()) {
            continue;
        }

        float similarity = cosineDistance(queryEmbedding, storedEmbedding);
        similarities.append(similarity);
        QString fileName = QFileInfo(vecPath).baseName();

        qDebug() << "Vector" << fileName << "similarity:" << similarity;

        // Предварительно сохраняем все результаты
        if (similarity > config.minAllowedThreshold) {
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

            preliminaryResults.append({result, similarity});
        }
    }

    // Вычисляем адаптивный порог
    float adaptiveThreshold = calculateAdaptiveThreshold(similarities, config);
    qDebug() << "Adaptive threshold:" << adaptiveThreshold;

    // Фильтруем результаты по адаптивному порогу
    QVector<SearchResult> results;
    for (const auto &[result, similarity] : preliminaryResults) {
        if (similarity >= adaptiveThreshold) {
            results.append(result);
        }
    }

    // Сортируем по убыванию схожести
    std::sort(results.begin(), results.end(), [](const auto &a, const auto &b) {
        return a.similarity > b.similarity;
    });

    // Ограничиваем количество результатов
    if (results.size() > config.maxResults) {
        results.resize(config.maxResults);
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
    if (a.size() != b.size()) {
        qWarning() << "Vectors have different sizes!";
        return 0.0f;
    }

    const float *ptrA = a.constData();
    const float *ptrB = b.constData();
    const int size = a.size();

    double dotProduct = 0.0;
    double normA = 0.0;
    double normB = 0.0;

    for (int i = 0; i < size; i++) {
        dotProduct += static_cast<double>(ptrA[i]) * static_cast<double>(ptrB[i]);
        normA += static_cast<double>(ptrA[i]) * static_cast<double>(ptrA[i]);
        normB += static_cast<double>(ptrB[i]) * static_cast<double>(ptrB[i]);
    }

    if (normA <= 1e-10 || normB <= 1e-10) {
        return 0.0f;
    }

    // Вычисляем косинусное сходство в диапазоне [-1, 1]
    double similarity = dotProduct / (std::sqrt(normA) * std::sqrt(normB));

    // Ограничиваем значение в диапазоне [-1, 1]
    similarity = std::max(-1.0, std::min(1.0, similarity));

    // Преобразуем в диапазон [0, 1]
    similarity = (similarity + 1.0) / 2.0;

    return similarity;
}

float EmbeddingsStorage::calculateAdaptiveThreshold(const QVector<float> &similarities,
                                                    const SearchConfig &config)
{
    if (similarities.isEmpty()) {
        return config.minSimilarity;
    }

    // Сортируем значения по убыванию
    auto sortedSims = similarities;
    std::sort(sortedSims.begin(), sortedSims.end(), std::greater<float>());

    // Если результатов меньше желаемого минимума, понижаем порог
    if (sortedSims.size() < config.minResultsCount) {
        return config.minAllowedThreshold;
    }

    // Ищем значительный разрыв в значениях
    float maxGap = 0.0f;
    int thresholdIndex = 0;

    for (int i = 0; i < sortedSims.size() - 1; ++i) {
        float gap = sortedSims[i] - sortedSims[i + 1];

        // Проверяем, является ли разрыв значимым
        if (gap > maxGap && gap > config.significantGap) {
            maxGap = gap;
            thresholdIndex = i;

            // Если после разрыва у нас есть достаточно результатов, используем его
            if (i + 1 >= config.minResultsCount) {
                break;
            }
        }
    }

    // Вычисляем адаптивный порог
    float adaptiveThreshold;
    if (maxGap > config.significantGap) {
        adaptiveThreshold = (sortedSims[thresholdIndex] + sortedSims[thresholdIndex + 1]) / 2.0f;
    } else {
        qsizetype index = std::min<qsizetype>(static_cast<qsizetype>(config.minResultsCount - 1),
                                              sortedSims.size() - 1);
        adaptiveThreshold = sortedSims[index];
    }

    // Ограничиваем порог заданными пределами
    return std::max(std::min(adaptiveThreshold, config.maxAllowedThreshold),
                    config.minAllowedThreshold);
}

} // namespace QodeAssist::LLMCore
