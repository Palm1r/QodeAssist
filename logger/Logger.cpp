#include "Logger.hpp"
#include <QtCore/qdebug.h>
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

    const QString prefixedMessage = QLatin1String("[Qode Assist] ") + message;
    qDebug() << prefixedMessage;
}

void Logger::logMessages(const QStringList &messages, bool silent)
{
    if (!m_loggingEnabled)
        return;

    QStringList prefixedMessages;
    for (const QString &message : messages) {
        prefixedMessages << (QLatin1String("[Qode Assist] ") + message);
    }

    qDebug() << prefixedMessages;
}
} // namespace QodeAssist
