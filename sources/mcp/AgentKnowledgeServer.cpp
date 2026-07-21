// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentKnowledgeServer.hpp"

#include <QHostAddress>
#include <QUuid>

#include <LLMQore/McpHttpServerTransport.hpp>
#include <LLMQore/McpServer.hpp>
#include <LLMQore/McpTypes.hpp>
#include <LLMQore/Version.hpp>

#include <logger/Logger.hpp>

#include "tools/EditorStateTools.hpp"
#include "tools/GetIssuesListTool.hpp"

namespace QodeAssist::Mcp {

namespace {

QString knowledgeServerName()
{
    return QStringLiteral("qodeassist");
}

} // namespace

AgentKnowledgeServer::AgentKnowledgeServer(QObject *parent)
    : QObject(parent)
{}

AgentKnowledgeServer::~AgentKnowledgeServer()
{
    stop();
}

QString AgentKnowledgeServer::serverName() const
{
    return knowledgeServerName();
}

void AgentKnowledgeServer::setIgnorePredicate(std::function<bool(const QString &)> predicate)
{
    m_ignorePredicate = std::move(predicate);
}

QStringList AgentKnowledgeServer::toolIds()
{
    return {
        Tools::ListOpenEditorsTool().id(),
        Tools::GetEditorSelectionTool().id(),
        Tools::GetProjectModelTool().id(),
        Tools::GetIssuesListTool().id()};
}

QString AgentKnowledgeServer::endpointUrl() const
{
    return m_runningPort == 0
               ? QString()
               : QStringLiteral("http://127.0.0.1:%1/mcp/%2").arg(m_runningPort).arg(m_pathToken);
}

QString AgentKnowledgeServer::start()
{
    if (m_runningPort != 0)
        return endpointUrl();

    m_pathToken = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ::LLMQore::Mcp::HttpServerConfig transportConfig;
    transportConfig.address = QHostAddress::LocalHost;
    transportConfig.port = 0;
    transportConfig.path = QStringLiteral("/mcp/%1").arg(m_pathToken);

    m_transport = new ::LLMQore::Mcp::McpHttpServerTransport(transportConfig, this);

    ::LLMQore::Mcp::McpServerConfig serverConfig;
    serverConfig.serverInfo = {"QodeAssist", QStringLiteral(LLMQORE_VERSION_STRING)};
    serverConfig.instructions = tr(
        "Read-only access to what Qt Creator knows about this session: files open in the editor "
        "with their unsaved contents, the current selection, build issues, and the configured "
        "kit and build directory. Use it for anything you cannot answer by reading the files on "
        "disk yourself.");

    m_server = new ::LLMQore::Mcp::McpServer(m_transport.data(), serverConfig, this);

    auto *openEditors = new Tools::ListOpenEditorsTool(m_server);
    openEditors->setIgnorePredicate(m_ignorePredicate);

    m_server->addTool(openEditors);
    m_server->addTool(new Tools::GetEditorSelectionTool(m_server));
    m_server->addTool(new Tools::GetProjectModelTool(m_server));
    m_server->addTool(new Tools::GetIssuesListTool(m_server));

    m_server->start();

    if (!m_transport->isOpen()) {
        LOG_MESSAGE("The QodeAssist knowledge server could not bind a loopback port");
        stop();
        return {};
    }

    m_runningPort = m_transport->serverPort();
    LOG_MESSAGE(QString("QodeAssist knowledge server listening on 127.0.0.1:%1").arg(m_runningPort));

    return endpointUrl();
}

void AgentKnowledgeServer::stop()
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
