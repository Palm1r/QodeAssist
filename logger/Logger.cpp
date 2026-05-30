// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "Logger.hpp"
#include <coreplugin/messagemanager.h>

namespace QodeAssist {

Logger &Logger::instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_loggingEnabled(false)
{}

void Logger::setLoggingEnabled(bool enable)
{
    m_loggingEnabled = enable;
}

void Logger::log(const QString &message, bool silent)
{
    if (!m_loggingEnabled)
        return;

    const QString prefixedMessage = QLatin1String("[QodeAssist] ") + message;
    if (silent) {
        Core::MessageManager::writeSilently(prefixedMessage);
    } else {
        Core::MessageManager::writeFlashing(prefixedMessage);
    }
}
} // namespace QodeAssist
