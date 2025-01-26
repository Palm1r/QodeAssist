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

#include "RAGSimilaritySearch.hpp"
#include "logger/Logger.hpp"

#include <cmath>

namespace QodeAssist::Context {

float RAGSimilaritySearch::l2Distance(const RAGVector &v1, const RAGVector &v2)
{
    if (v1.size() != v2.size()) {
        LOG_MESSAGE(QString("Vector size mismatch: %1 vs %2").arg(v1.size()).arg(v2.size()));
        return std::numeric_limits<float>::max();
    }

    float sum = 0.0f;
    for (size_t i = 0; i < v1.size(); ++i) {
        float diff = v1[i] - v2[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

float RAGSimilaritySearch::cosineSimilarity(const RAGVector &v1, const RAGVector &v2)
{
    if (v1.size() != v2.size()) {
        LOG_MESSAGE(QString("Vector size mismatch: %1 vs %2").arg(v1.size()).arg(v2.size()));
        return 0.0f;
    }

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

    if (norm1 == 0.0f || norm2 == 0.0f)
        return 0.0f;
    return dotProduct / (norm1 * norm2);
}

} // namespace QodeAssist::Context
