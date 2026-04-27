// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>

namespace LLMQore::Mcp {
class McpServer;
class McpHttpServerTransport;
} // namespace LLMQore::Mcp

namespace QodeAssist::Mcp {

class McpServerManager : public QObject
{
    Q_OBJECT
public:
    explicit McpServerManager(QObject *parent = nullptr);
    ~McpServerManager() override;

    void init();

private:
    void reconfigure();
    void startServer();
    void stopServer();

    QPointer<::LLMQore::Mcp::McpServer> m_server;
    QPointer<::LLMQore::Mcp::McpHttpServerTransport> m_transport;
    quint16 m_runningPort = 0;
};

} // namespace QodeAssist::Mcp
