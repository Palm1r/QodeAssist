#include "Logger.hpp"

#include <QLatin1String>

#include <coreplugin/messagemanager.h>

namespace QodeAssist {

QtMessageHandler Logger::s_previousHandler = nullptr;

Logger &Logger::instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_loggingEnabled(false)
{
    installMessageHandler();
}

void Logger::installMessageHandler()
{
    s_previousHandler = qInstallMessageHandler(qtMessageHandler);
}

void Logger::qtMessageHandler(QtMsgType type,
                              const QMessageLogContext &context,
                              const QString &msg)
{
    if (context.category
        && QLatin1String(context.category).startsWith(QLatin1String("qodeassist."))) {
        const QString prefixed = QLatin1String("[") + QLatin1String(context.category)
                                 + QLatin1String("] ") + msg;
        instance().log(prefixed);
        return;
    }

    if (s_previousHandler)
        s_previousHandler(type, context, msg);
}

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
