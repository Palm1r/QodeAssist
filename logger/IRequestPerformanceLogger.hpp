// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace QodeAssist {

class IRequestPerformanceLogger
{
public:
    virtual ~IRequestPerformanceLogger() = default;

    virtual void startTimeMeasurement(const QString &requestId) = 0;
    virtual void endTimeMeasurement(const QString &requestId) = 0;
    virtual void logPerformance(const QString &requestId, qint64 elapsedMs) = 0;
};

} // namespace QodeAssist
