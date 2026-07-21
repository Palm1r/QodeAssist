// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>

#include <QObject>
#include <QPointer>
#include <QStringList>

#include "acp/AgentKnowledgeService.hpp"

namespace LLMQore::Mcp {
class McpServer;
class McpHttpServerTransport;
} // namespace LLMQore::Mcp

namespace QodeAssist::Mcp {

class AgentKnowledgeServer : public QObject, public Acp::AgentKnowledgeService
{
    Q_OBJECT

public:
    explicit AgentKnowledgeServer(QObject *parent = nullptr);
    ~AgentKnowledgeServer() override;

    QString start() override;
    void stop() override;
    QString serverName() const override;

    void setIgnorePredicate(std::function<bool(const QString &)> predicate);

    quint16 runningPort() const { return m_runningPort; }
    QString endpointUrl() const;
    static QStringList toolIds();

private:
    QPointer<::LLMQore::Mcp::McpServer> m_server;
    QPointer<::LLMQore::Mcp::McpHttpServerTransport> m_transport;
    std::function<bool(const QString &)> m_ignorePredicate;
    QString m_pathToken;
    quint16 m_runningPort = 0;
};

} // namespace QodeAssist::Mcp
