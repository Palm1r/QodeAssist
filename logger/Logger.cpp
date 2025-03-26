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

bool Logger::isLoggingEnabled() const
{
    return m_loggingEnabled;
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

void Logger::logMessages(const QStringList &messages, bool silent)
{
    if (!m_loggingEnabled)
        return;

    QStringList prefixedMessages;
    for (const QString &message : messages) {
        prefixedMessages << (QLatin1String("[QodeAssist] ") + message);
    }

    if (silent) {
        Core::MessageManager::writeSilently(prefixedMessages);
    } else {
        Core::MessageManager::writeFlashing(prefixedMessages);
    }
}
} // namespace QodeAssist
