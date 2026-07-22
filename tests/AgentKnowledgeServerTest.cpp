// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentKnowledgeServerTest.hpp"

#include <QHostAddress>
#include <QSignalSpy>
#include <QTest>

#include <LLMQore/ToolsManager.hpp>

#include "acp/AgentKnowledgeService.hpp"
#include "mcp/AgentKnowledgeServer.hpp"
#include "tools/BuildProjectTool.hpp"
#include "tools/CreateNewFileTool.hpp"
#include "tools/EditFileTool.hpp"
#include "tools/ExecuteTerminalCommandTool.hpp"
#include "tools/ReadFileTool.hpp"

namespace QodeAssist {

void AgentKnowledgeServerTest::testKnowledgeServerExposesOnlyReadOnlyTools()
{
    const QStringList exposed = Mcp::AgentKnowledgeServer::toolIds();

    QCOMPARE(
        QSet<QString>(exposed.cbegin(), exposed.cend()),
        (QSet<QString>{
            "list_open_editors", "get_editor_selection", "get_project_model", "get_issues_list"}));

    const QStringList mutating{
        Tools::EditFileTool().id(),
        Tools::CreateNewFileTool().id(),
        Tools::ExecuteTerminalCommandTool().id(),
        Tools::BuildProjectTool().id()};

    for (const QString &tool : mutating)
        QVERIFY2(!exposed.contains(tool), qPrintable(tool));
}

void AgentKnowledgeServerTest::testKnowledgeServerBindsLoopbackOnAnEphemeralPort()
{
    Mcp::AgentKnowledgeServer server;

    const QString url = server.start();
    QVERIFY(!url.isEmpty());
    QVERIFY(server.runningPort() != 0);

    const QUrl parsed(url);
    QVERIFY(QHostAddress(parsed.host()).isLoopback());
    QCOMPARE(parsed.port(), int(server.runningPort()));

    QVERIFY2(
        parsed.path().length() > QStringLiteral("/mcp/").length(),
        qPrintable(parsed.path()));

    QCOMPARE(server.start(), url);

    server.stop();
    QCOMPARE(server.runningPort(), quint16(0));
    server.stop();
}

} // namespace QodeAssist
