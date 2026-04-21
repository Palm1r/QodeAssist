// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

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
