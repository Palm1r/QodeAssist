#include "EnhancedRAGSimilaritySearch.hpp"

#include <QSet>

namespace QodeAssist::Context {

// Static regex getters
const QRegularExpression &EnhancedRAGSimilaritySearch::getNamespaceRegex()
{
    static const QRegularExpression regex(R"(namespace\s+(?:\w+\s*::\s*)*\w+\s*\{)");
    return regex;
}

const QRegularExpression &EnhancedRAGSimilaritySearch::getClassRegex()
{
    static const QRegularExpression regex(
        R"((?:template\s*<[^>]*>\s*)?(?:class|struct)\s+(\w+)\s*(?:final\s*)?(?::\s*(?:public|protected|private)\s+\w+(?:\s*,\s*(?:public|protected|private)\s+\w+)*\s*)?{)");
    return regex;
}

const QRegularExpression &EnhancedRAGSimilaritySearch::getFunctionRegex()
{
    static const QRegularExpression regex(
        R"((?:virtual\s+)?(?:static\s+)?(?:inline\s+)?(?:explicit\s+)?(?:constexpr\s+)?(?:[\w:]+\s+)?(?:\w+\s*::\s*)*\w+\s*\([^)]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?(?:=\s*0\s*)?(?:=\s*default\s*)?(?:=\s*delete\s*)?(?:\s*->.*?)?\s*{)");
    return regex;
}

const QRegularExpression &EnhancedRAGSimilaritySearch::getTemplateRegex()
{
    static const QRegularExpression regex(R"(template\s*<[^>]*>\s*(?:class|struct|typename)\s+\w+)");
    return regex;
}

// Cache getters
QCache<QString, EnhancedRAGSimilaritySearch::SimilarityScore> &
EnhancedRAGSimilaritySearch::getScoreCache()
{
    static QCache<QString, SimilarityScore> cache(1000); // Cache size of 1000 entries
    return cache;
}

QCache<QString, QStringList> &EnhancedRAGSimilaritySearch::getStructureCache()
{
    static QCache<QString, QStringList> cache(500); // Cache size of 500 entries
    return cache;
}

// Main public interface
EnhancedRAGSimilaritySearch::SimilarityScore EnhancedRAGSimilaritySearch::calculateSimilarity(
    const RAGVector &v1, const RAGVector &v2, const QString &code1, const QString &code2)
{
    // Generate cache key based on content hashes
    QString cacheKey = QString("%1_%2").arg(qHash(code1)).arg(qHash(code2));

    // Check cache first
    auto &scoreCache = getScoreCache();
    if (auto *cached = scoreCache.object(cacheKey)) {
        return *cached;
    }

    // Calculate new similarity score
    SimilarityScore score = calculateSimilarityInternal(v1, v2, code1, code2);

    // Cache the result
    scoreCache.insert(cacheKey, new SimilarityScore(score));

    return score;
}

// Internal implementation
EnhancedRAGSimilaritySearch::SimilarityScore EnhancedRAGSimilaritySearch::calculateSimilarityInternal(
    const RAGVector &v1, const RAGVector &v2, const QString &code1, const QString &code2)
{
    if (v1.empty() || v2.empty()) {
        LOG_MESSAGE("Warning: Empty vectors in similarity calculation");
        return SimilarityScore(0.0f, 0.0f, 0.0f);
    }

    if (v1.size() != v2.size()) {
        LOG_MESSAGE(QString("Vector size mismatch: %1 vs %2").arg(v1.size()).arg(v2.size()));
        return SimilarityScore(0.0f, 0.0f, 0.0f);
    }

    // Calculate semantic similarity using vector embeddings
    float semantic_similarity = 0.0f;

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64)
    if (v1.size() >= 4) { // Use SSE for vectors of 4 or more elements
        semantic_similarity = calculateCosineSimilaritySSE(v1, v2);
    } else {
        semantic_similarity = calculateCosineSimilarity(v1, v2);
    }
#else
    semantic_similarity = calculateCosineSimilarity(v1, v2);
#endif

    // If semantic similarity is very low, skip structural analysis
    if (semantic_similarity < 0.0001f) {
        return SimilarityScore(0.0f, 0.0f, 0.0f);
    }

    // Calculate structural similarity
    float structural_similarity = calculateStructuralSimilarity(code1, code2);

    // Calculate combined score with dynamic weights
    float semantic_weight = 0.7f;
    const int large_file_threshold = 10000;

    if (code1.size() > large_file_threshold || code2.size() > large_file_threshold) {
        semantic_weight = 0.8f; // Increase semantic weight for large files
    }

    float combined_score = semantic_weight * semantic_similarity
                           + (1.0f - semantic_weight) * structural_similarity;

    return SimilarityScore(semantic_similarity, structural_similarity, combined_score);
}

float EnhancedRAGSimilaritySearch::calculateCosineSimilarity(const RAGVector &v1, const RAGVector &v2)
{
    float dotProduct = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (size_t i = 0; i < v1.size(); ++i) {
        dotProduct += v1[i] * v2[i];
        norm1 += v1[i] * v1[i];
        norm2 += v2[i] * v2[i];
    }

    norm1 = std::sqrt(norm1);
    norm2 = std::sqrt(norm2);

    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (norm1 * norm2);
}

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64)
float EnhancedRAGSimilaritySearch::calculateCosineSimilaritySSE(
    const RAGVector &v1, const RAGVector &v2)
{
    const float *p1 = v1.data();
    const float *p2 = v2.data();
    const size_t size = v1.size();
    const size_t alignedSize = size & ~3ULL; // Round down to multiple of 4

    __m128 sum = _mm_setzero_ps();
    __m128 norm1 = _mm_setzero_ps();
    __m128 norm2 = _mm_setzero_ps();

    // Process 4 elements at a time using SSE
    for (size_t i = 0; i < alignedSize; i += 4) {
        __m128 v1_vec = _mm_loadu_ps(p1 + i); // Use unaligned load for safety
        __m128 v2_vec = _mm_loadu_ps(p2 + i);

        sum = _mm_add_ps(sum, _mm_mul_ps(v1_vec, v2_vec));
        norm1 = _mm_add_ps(norm1, _mm_mul_ps(v1_vec, v1_vec));
        norm2 = _mm_add_ps(norm2, _mm_mul_ps(v2_vec, v2_vec));
    }

    float dotProduct = horizontalSum(sum);
    float n1 = std::sqrt(horizontalSum(norm1));
    float n2 = std::sqrt(horizontalSum(norm2));

    // Process remaining elements
    for (size_t i = alignedSize; i < size; ++i) {
        dotProduct += v1[i] * v2[i];
        n1 += v1[i] * v1[i];
        n2 += v2[i] * v2[i];
    }

    if (n1 == 0.0f || n2 == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(n1) * std::sqrt(n2));
}

float EnhancedRAGSimilaritySearch::horizontalSum(__m128 x)
{
    __m128 shuf = _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(x, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}
#endif

float EnhancedRAGSimilaritySearch::calculateStructuralSimilarity(
    const QString &code1, const QString &code2)
{
    QStringList structures1 = extractStructures(code1);
    QStringList structures2 = extractStructures(code2);

    return calculateJaccardSimilarity(structures1, structures2);
}

QStringList EnhancedRAGSimilaritySearch::extractStructures(const QString &code)
{
    // Check cache first
    auto &structureCache = getStructureCache();
    QString cacheKey = QString::number(qHash(code));

    if (auto *cached = structureCache.object(cacheKey)) {
        return *cached;
    }

    QStringList structures;
    structures.reserve(100); // Reserve space for typical file

    // Extract namespaces
    auto namespaceMatches = getNamespaceRegex().globalMatch(code);
    while (namespaceMatches.hasNext()) {
        structures.append(namespaceMatches.next().captured().trimmed());
    }

    // Extract classes
    auto classMatches = getClassRegex().globalMatch(code);
    while (classMatches.hasNext()) {
        structures.append(classMatches.next().captured().trimmed());
    }

    // Extract functions
    auto functionMatches = getFunctionRegex().globalMatch(code);
    while (functionMatches.hasNext()) {
        structures.append(functionMatches.next().captured().trimmed());
    }

    // Extract templates
    auto templateMatches = getTemplateRegex().globalMatch(code);
    while (templateMatches.hasNext()) {
        structures.append(templateMatches.next().captured().trimmed());
    }

    // Cache the result
    structureCache.insert(cacheKey, new QStringList(structures));

    return structures;
}

float EnhancedRAGSimilaritySearch::calculateJaccardSimilarity(
    const QStringList &set1, const QStringList &set2)
{
    if (set1.isEmpty() && set2.isEmpty()) {
        return 1.0f; // Пустые множества считаем идентичными
    }
    if (set1.isEmpty() || set2.isEmpty()) {
        return 0.0f;
    }

    QSet<QString> set1Unique = QSet<QString>(set1.begin(), set1.end());
    QSet<QString> set2Unique = QSet<QString>(set2.begin(), set2.end());

    QSet<QString> intersection = set1Unique;
    intersection.intersect(set2Unique);

    QSet<QString> union_set = set1Unique;
    union_set.unite(set2Unique);

    return static_cast<float>(intersection.size()) / union_set.size();
}
} // namespace QodeAssist::Context
