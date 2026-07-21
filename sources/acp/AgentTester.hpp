// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "acp/AgentDefinition.hpp"

namespace LLMQore::Acp {
class AcpClient;
}

namespace LLMQore::Rpc {
class StdioClientTransport;
}

namespace QodeAssist::Acp {

class AgentTester : public QObject
{
    Q_OBJECT

public:
    explicit AgentTester(QObject *parent = nullptr);

    void start(const AgentDefinition &agent, const QString &workingDirectory);
    void cancel();

    bool isRunning() const { return m_running; }

signals:
    void finished(bool ok, const QString &report);

private:
    void report(bool ok, const QString &text);
    void releaseClient();
    QString diagnostics() const;

    LLMQore::Acp::AcpClient *m_client = nullptr;
    LLMQore::Rpc::StdioClientTransport *m_transport = nullptr;
    QStringList m_stderr;
    QString m_runner;
    bool m_running = false;
};

} // namespace QodeAssist::Acp
