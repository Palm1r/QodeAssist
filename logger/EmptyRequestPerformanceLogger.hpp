// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "IRequestPerformanceLogger.hpp"

namespace QodeAssist {

class EmptyRequestPerformanceLogger : public IRequestPerformanceLogger
{
public:
    void startTimeMeasurement(const QString &requestId) override {}
    void endTimeMeasurement(const QString &requestId) override {}
    void logPerformance(const QString &requestId, qint64 elapsedMs) override {}
};

} // namespace QodeAssist
