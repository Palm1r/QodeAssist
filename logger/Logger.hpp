// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QString>

namespace QodeAssist {

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger &instance();

    void setLoggingEnabled(bool enable);

    void log(const QString &message, bool silent = true);

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    bool m_loggingEnabled;
};

#define LOG_MESSAGE(msg) QodeAssist::Logger::instance().log(msg)

} // namespace QodeAssist
