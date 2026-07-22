// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AcpCompletionEngineTest.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QTextDocument>

#include <LLMQore/AcpClient.hpp>

#include "CompletionTestSupport.hpp"
#include "FakeAcpAgent.hpp"
#include "acp/AgentDefinition.hpp"
#include "acp/AgentSpawn.hpp"
#include "completion/AcpCompletionEngine.hpp"
#include "mcp/CompletionMcpServer.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "tools/ProposeCompletionTool.hpp"

namespace QodeAssist {

namespace {

struct AcpCompletionFixture
{
    AcpCompletionFixture()
        : settings(new Settings::CodeCompletionSettings)
        , engine(
              *settings,
              Settings::generalSettings(),
              [this](const QString &id) -> std::optional<Acp::AgentDefinition> {
                  if (agentAvailable && id == agentId)
                      return makeAgent();
                  return std::nullopt;
              },
              reader,
              &proposeTool)
    {
        document.setPlainText("int main() {\n    return 0;\n}\n");
        reader.document = &document;

        settings->completionAgentId.setValue(agentId, Utils::BaseAspect::BeQuiet);

        engine.setClientFactory(
            [this](const Acp::AgentDefinition &, const QString &cwd, QObject *parent) {
                spawnedCwd = cwd;
                fakeAgent = new FakeAcpAgent(parent);
                return Acp::AgentProcess{
                    new LLMQore::Acp::AcpClient(
                        fakeAgent,
                        {QStringLiteral("QodeAssistTest"), QStringLiteral("1.0"), QString()},
                        parent),
                    QStringLiteral("fake")};
            });
    }

    static Acp::AgentDefinition makeAgent()
    {
        Acp::AgentDefinition agent;
        agent.id = "fake-agent";
        agent.name = "Fake Agent";
        agent.distribution.kind = Acp::AgentDistributionKind::Command;
        agent.distribution.command = "/bin/true";
        return agent;
    }

    QString agentId = "fake-agent";
    bool agentAvailable = true;
    QString spawnedCwd;
    FakeAcpAgent *fakeAgent = nullptr;
    FakeCompletionDocumentReader reader;
    QTextDocument document;
    QSharedPointer<Settings::CodeCompletionSettings> settings;
    Tools::ProposeCompletionTool proposeTool;
    AcpCompletionEngine engine;
};

} // anonymous namespace

void AcpCompletionEngineTest::testAcpCompletionFailsWithoutAgent()
{
    AcpCompletionFixture fixture;
    fixture.settings->completionAgentId.setValue(QString(), Utils::BaseAspect::BeQuiet);

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    QVERIFY(!fixture.engine.hasConfiguredAgent());
    fixture.engine.request(31, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    QVERIFY(fixture.fakeAgent == nullptr);
    QCOMPARE(ready.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toULongLong(), quint64(31));
    QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("no agent selected")));
}

void AcpCompletionEngineTest::testAcpCompletionRoutesProposalToGhostText()
{
    AcpCompletionFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    QVERIFY(fixture.engine.hasConfiguredAgent());
    fixture.engine.request(33, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    const auto result = fixture.proposeTool
                            .executeAsync(
                                QJsonObject{
                                    {"file", "/path/to/file.cpp"}, {"text", "return 42;"}})
                            .result();
    QVERIFY(!result.isError);

    QCOMPARE(failed.count(), 0);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(33));
    QCOMPARE(ready.first().at(1).toString(), QStringLiteral("return 42;"));

    const auto stale = fixture.proposeTool
                           .executeAsync(
                               QJsonObject{{"file", "/path/to/file.cpp"}, {"text", "again;"}})
                           .result();
    QVERIFY(!stale.isError);
    QCOMPARE(ready.count(), 1);
}

void AcpCompletionEngineTest::testAcpCompletionOffersMcpServerToTheAgent()
{
    AcpCompletionFixture fixture;

    fixture.engine.request(34, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    QVERIFY(fixture.fakeAgent != nullptr);
    QTRY_VERIFY(!fixture.fakeAgent->offeredMcpServers().isEmpty());

    const QJsonArray servers = fixture.fakeAgent->offeredMcpServers();
    QCOMPARE(servers.size(), 1);
    const QJsonObject server = servers.first().toObject();
    QCOMPARE(server.value("name").toString(), Mcp::CompletionMcpServer::serverName());
    QVERIFY(server.value("url").toString().startsWith(QStringLiteral("http://127.0.0.1:")));
    QTRY_VERIFY(!fixture.fakeAgent->prompts().isEmpty());
    const QString promptText = fixture.fakeAgent->prompts().first().first().toObject()
                                   .value("text").toString();
    QVERIFY(promptText.contains(QStringLiteral("propose_completion")));
}

void AcpCompletionEngineTest::testAcpCompletionSupersedesInflightRequest()
{
    AcpCompletionFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(35, {QStringLiteral("/path/to/file.cpp"), 1, 4});
    fixture.engine.request(36, {QStringLiteral("/path/to/file.cpp"), 2, 0});

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toULongLong(), quint64(35));
    QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("Superseded")));

    const auto result = fixture.proposeTool
                            .executeAsync(
                                QJsonObject{{"file", "/path/to/file.cpp"}, {"text", "done;"}})
                            .result();
    QVERIFY(!result.isError);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(36));
}

} // namespace QodeAssist
