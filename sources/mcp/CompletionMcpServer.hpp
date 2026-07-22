// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

namespace LLMQore {
class BaseTool;
namespace Mcp {
class McpServer;
class McpHttpServerTransport;
} // namespace Mcp
} // namespace LLMQore

namespace QodeAssist::Mcp {

class CompletionMcpServer : public QObject
{
    Q_OBJECT

public:
    explicit CompletionMcpServer(QObject *parent = nullptr);
    ~CompletionMcpServer() override;

    QString start(::LLMQore::BaseTool *proposeTool);
    void stop();

    static QString serverName();
    QString endpointUrl() const;
    quint16 runningPort() const { return m_runningPort; }

private:
    QPointer<::LLMQore::Mcp::McpServer> m_server;
    QPointer<::LLMQore::Mcp::McpHttpServerTransport> m_transport;
    QString m_pathToken;
    quint16 m_runningPort = 0;
};

} // namespace QodeAssist::Mcp
