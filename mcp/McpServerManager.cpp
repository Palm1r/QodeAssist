// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "McpServerManager.hpp"

#include <LLMQore/BaseTool.hpp>
#include <LLMQore/McpHttpServerTransport.hpp>
#include <LLMQore/McpServer.hpp>
#include <LLMQore/McpTypes.hpp>
#include <LLMQore/Version.hpp>

#include <QHostAddress>

#include <logger/Logger.hpp>
#include <settings/McpSettings.hpp>

#include "tools/BuildProjectTool.hpp"
#include "tools/CreateNewFileTool.hpp"
#include "tools/EditFileTool.hpp"
#include "tools/ExecuteTerminalCommandTool.hpp"
#include "tools/FindFileTool.hpp"
#include "tools/GetIssuesListTool.hpp"
#include "tools/ListProjectFilesTool.hpp"
#include "tools/ProjectSearchTool.hpp"
#include "tools/ReadFileTool.hpp"
#include "tools/TodoTool.hpp"

namespace QodeAssist::Mcp {

McpServerManager::McpServerManager(QObject *parent)
    : QObject(parent)
{}

McpServerManager::~McpServerManager()
{
    stopServer();
}

void McpServerManager::init()
{
    auto &s = Settings::mcpSettings();

    auto onChanged = [this] { reconfigure(); };
    connect(&s.enableMcpServer, &Utils::BaseAspect::changed, this, onChanged);
    connect(&s.mcpServerPort, &Utils::BaseAspect::changed, this, onChanged);

    reconfigure();
}

void McpServerManager::reconfigure()
{
    auto &s = Settings::mcpSettings();
    const bool wantRunning = s.enableMcpServer();
    const quint16 wantPort = static_cast<quint16>(s.mcpServerPort());

    if (wantRunning && m_server && m_runningPort == wantPort)
        return;

    stopServer();

    if (wantRunning) {
        startServer();
    }
}

void McpServerManager::startServer()
{
    auto &s = Settings::mcpSettings();

    ::LLMQore::Mcp::HttpServerConfig cfg;
    cfg.address = QHostAddress::LocalHost;
    cfg.port = static_cast<quint16>(s.mcpServerPort());
    cfg.path = QStringLiteral("/mcp");

    m_transport = new ::LLMQore::Mcp::McpHttpServerTransport(cfg, this);

    ::LLMQore::Mcp::McpServerConfig scfg;
    scfg.serverInfo = {"QodeAssist",
                       QStringLiteral(LLMQORE_VERSION_STRING)};
    scfg.instructions = tr("QodeAssist MCP server exposing Qt Creator project tools.");

    m_server = new ::LLMQore::Mcp::McpServer(m_transport.data(), scfg, this);

    m_server->addTool(new Tools::ListProjectFilesTool(m_server));
    m_server->addTool(new Tools::FindFileTool(m_server));
    m_server->addTool(new Tools::ReadFileTool(m_server));
    m_server->addTool(new Tools::ProjectSearchTool(m_server));
    m_server->addTool(new Tools::CreateNewFileTool(m_server));
    m_server->addTool(new Tools::EditFileTool(m_server));
    m_server->addTool(new Tools::BuildProjectTool(m_server));
    m_server->addTool(new Tools::GetIssuesListTool(m_server));
    m_server->addTool(new Tools::ExecuteTerminalCommandTool(m_server));
    m_server->addTool(new Tools::TodoTool(m_server));

    m_server->start();

    if (!m_transport->isOpen()) {
        LOG_MESSAGE(QString("QodeAssist MCP server failed to start on port %1").arg(cfg.port));
        stopServer();
        return;
    }

    m_runningPort = m_transport->serverPort();
    LOG_MESSAGE(QString("QodeAssist MCP server listening on http://127.0.0.1:%1/mcp")
                    .arg(m_runningPort));
}

void McpServerManager::stopServer()
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
}

} // namespace QodeAssist::Mcp
