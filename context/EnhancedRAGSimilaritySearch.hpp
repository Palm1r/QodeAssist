#pragma once

#include <QCache>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

#include "RAGData.hpp"
#include "logger/Logger.hpp"

namespace QodeAssist::Context {

class EnhancedRAGSimilaritySearch
{
public:
    struct SimilarityScore
    {
        float semantic_similarity{0.0f};
        float structural_similarity{0.0f};
        float combined_score{0.0f};

        SimilarityScore() = default;
        SimilarityScore(float sem, float str, float comb)
            : semantic_similarity(sem)
            , structural_similarity(str)
            , combined_score(comb)
        {}
    };

    static SimilarityScore calculateSimilarity(
        const RAGVector &v1, const RAGVector &v2, const QString &code1, const QString &code2);

private:
    static SimilarityScore calculateSimilarityInternal(
        const RAGVector &v1, const RAGVector &v2, const QString &code1, const QString &code2);

    static float calculateCosineSimilarity(const RAGVector &v1, const RAGVector &v2);

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64)
    static float calculateCosineSimilaritySSE(const RAGVector &v1, const RAGVector &v2);
    static float horizontalSum(__m128 x);
#endif

    static float calculateStructuralSimilarity(const QString &code1, const QString &code2);
    static QStringList extractStructures(const QString &code);
    static float calculateJaccardSimilarity(const QStringList &set1, const QStringList &set2);

    static const QRegularExpression &getNamespaceRegex();
    static const QRegularExpression &getClassRegex();
    static const QRegularExpression &getFunctionRegex();
    static const QRegularExpression &getTemplateRegex();

    // Cache for similarity scores
    static QCache<QString, SimilarityScore> &getScoreCache();

    // Cache for extracted structures
    static QCache<QString, QStringList> &getStructureCache();

    EnhancedRAGSimilaritySearch() = delete; // Prevent instantiation
};

} // namespace QodeAssist::Context
