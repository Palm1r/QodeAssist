// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "CompletionMcpServer.hpp"

#include <QHostAddress>
#include <QUuid>

#include <LLMQore/BaseTool.hpp>
#include <LLMQore/McpHttpServerTransport.hpp>
#include <LLMQore/McpServer.hpp>
#include <LLMQore/McpTypes.hpp>
#include <LLMQore/Version.hpp>

#include <logger/Logger.hpp>

namespace QodeAssist::Mcp {

CompletionMcpServer::CompletionMcpServer(QObject *parent)
    : QObject(parent)
{}

CompletionMcpServer::~CompletionMcpServer()
{
    stop();
}

QString CompletionMcpServer::serverName()
{
    return QStringLiteral("qodeassist-completion");
}

QString CompletionMcpServer::endpointUrl() const
{
    return m_runningPort == 0
               ? QString()
               : QStringLiteral("http://127.0.0.1:%1/mcp/%2").arg(m_runningPort).arg(m_pathToken);
}

QString CompletionMcpServer::start(::LLMQore::BaseTool *proposeTool)
{
    if (!proposeTool)
        return {};

    if (m_runningPort != 0)
        return endpointUrl();

    m_pathToken = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ::LLMQore::Mcp::HttpServerConfig transportConfig;
    transportConfig.address = QHostAddress::LocalHost;
    transportConfig.port = 0;
    transportConfig.path = QStringLiteral("/mcp/%1").arg(m_pathToken);

    m_transport = new ::LLMQore::Mcp::McpHttpServerTransport(transportConfig, this);

    ::LLMQore::Mcp::McpServerConfig serverConfig;
    serverConfig.serverInfo = {"QodeAssist Completion", QStringLiteral(LLMQORE_VERSION_STRING)};
    serverConfig.instructions = tr(
        "Inline code-completion channel. Call propose_completion exactly once with the completion "
        "text for the requested cursor position; it is shown to the user as a ghost-text "
        "suggestion. Do not edit files.");

    m_server = new ::LLMQore::Mcp::McpServer(m_transport.data(), serverConfig, this);
    m_server->addTool(proposeTool);

    connect(
        m_server,
        &::LLMQore::Mcp::McpServer::toolCallReceived,
        this,
        [](const QString &toolName) {
            LOG_MESSAGE(
                QString("Completion MCP server: incoming tools/call for '%1'").arg(toolName));
        });

    m_server->start();

    if (!m_transport->isOpen()) {
        LOG_MESSAGE("The QodeAssist completion MCP server could not bind a loopback port");
        stop();
        return {};
    }

    m_runningPort = m_transport->serverPort();
    LOG_MESSAGE(
        QString("QodeAssist completion MCP server listening on %1").arg(endpointUrl()));

    return endpointUrl();
}

void CompletionMcpServer::stop()
{
    if (m_server) {
        m_server->stop();
        m_server->deleteLater();
        m_server.clear();
    }

    if (m_transport) {
        m_transport->deleteLater();
        m_transport.clear();
    }

    m_runningPort = 0;
    m_pathToken.clear();
}

} // namespace QodeAssist::Mcp
