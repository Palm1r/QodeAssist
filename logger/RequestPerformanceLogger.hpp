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
