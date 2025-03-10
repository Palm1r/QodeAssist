/*
 * Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
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

#include "RequestPerformanceLogger.hpp"
#include "Logger.hpp"

namespace QodeAssist {

void RequestPerformanceLogger::startTimeMeasurement(const QString &requestId)
{
    m_requestStartTimes[requestId] = QDateTime::currentMSecsSinceEpoch();
}

void RequestPerformanceLogger::endTimeMeasurement(const QString &requestId)
{
    if (!m_requestStartTimes.contains(requestId)) {
        return;
    }

    qint64 startTime = m_requestStartTimes[requestId];
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();
    qint64 totalTime = endTime - startTime;
    logPerformance(requestId, totalTime);
    m_requestStartTimes.remove(requestId);
}

void RequestPerformanceLogger::logPerformance(const QString &requestId, qint64 elapsedMs)
{
    LOG_MESSAGE(
        QString("Performance: %1 total completion time took %2 ms").arg(requestId).arg(elapsedMs));
}

void RequestPerformanceLogger::logPerformance(
    const QString &requestId, const QString &operation, qint64 elapsedMs)
{
    LOG_MESSAGE(QString("Performance: %1 %2 took %3 ms").arg(requestId, operation).arg(elapsedMs));
}

} // namespace QodeAssist
