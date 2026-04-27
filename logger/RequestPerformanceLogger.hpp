// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "IRequestPerformanceLogger.hpp"
#include <QDateTime>
#include <QMap>

namespace QodeAssist {

class RequestPerformanceLogger : public IRequestPerformanceLogger
{
public:
    RequestPerformanceLogger() = default;
    ~RequestPerformanceLogger() override = default;

    void startTimeMeasurement(const QString &requestId) override;
    void endTimeMeasurement(const QString &requestId) override;
    void logPerformance(const QString &requestId, qint64 elapsedMs) override;
    void logPerformance(const QString &requestId, const QString &operation, qint64 elapsedMs);

private:
    QMap<QString, qint64> m_requestStartTimes;
};

} // namespace QodeAssist
