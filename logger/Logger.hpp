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
    bool isLoggingEnabled() const;

    void log(const QString &message, bool silent = true);
    void logMessages(const QStringList &messages, bool silent = true);

    void installMessageHandler();

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    static void qtMessageHandler(QtMsgType type,
                                 const QMessageLogContext &context,
                                 const QString &msg);

    bool m_loggingEnabled;
    static QtMessageHandler s_previousHandler;
};

#define LOG_MESSAGE(msg) QodeAssist::Logger::instance().log(msg)
#define LOG_MESSAGES(msgs) QodeAssist::Logger::instance().logMessages(msgs)

} // namespace QodeAssist
