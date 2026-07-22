// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AcpChatBackendTest.hpp"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <LLMQore/AcpClient.hpp>

#include "ChatView/ChatFileStore.hpp"
#include "FakeAcpAgent.hpp"
#include "acp/AcpChatBackend.hpp"
#include "acp/AgentBinding.hpp"
#include "acp/AgentDefinition.hpp"
#include "acp/AgentKnowledgeService.hpp"
#include "acp/AgentSpawn.hpp"
#include "mcp/AgentKnowledgeServer.hpp"
#include "session/Session.hpp"
#include "session/SessionEvent.hpp"

namespace QodeAssist {

namespace {

Acp::AgentDefinition fakeAgentDefinition()
{
    Acp::AgentDefinition agent;
    agent.id = "fake-agent";
    agent.name = "Fake Agent";
    agent.distribution.kind = Acp::AgentDistributionKind::Command;
    agent.distribution.command = "/bin/true";
    return agent;
}

class AcpBackendFixture
{
public:
    AcpBackendFixture()
    {
        backend.setClientFactory(
            [this](const Acp::AgentDefinition &, const QString &cwd, QObject *parent) {
                requestedCwd = cwd;
                agent = new FakeAcpAgent(parent);
                configure(agent);
                return Acp::AgentProcess{
                    new LLMQore::Acp::AcpClient(
                        agent,
                        {QStringLiteral("QodeAssistTest"), QStringLiteral("1.0"), QString()},
                        parent),
                    QStringLiteral("fake")};
            });

        QObject::connect(
            &backend,
            &Session::ChatBackend::sessionEvent,
            &backend,
            [this](const Session::SessionEvent &event) { events.append(event); });

        backend.setStoredContentLoader([this](const QString &, const QString &storedPath) {
            return storage.value(storedPath);
        });

        backend.bindAgent(fakeAgentDefinition());
    }

    void send(const QString &text)
    {
        Session::TurnRequest request;
        request.userBlocks = {Session::TextBlock{text}};
        backend.sendTurn(request);
    }

    void sendBlocks(
        const QList<Session::ContentBlock> &blocks,
        const std::optional<Session::TurnContext> &context = std::nullopt)
    {
        Session::TurnRequest request;
        request.userBlocks = blocks;
        request.context = context;
        backend.sendTurn(request);
    }

    QList<QJsonObject> promptBlocksOfType(const QString &type) const
    {
        QList<QJsonObject> matching;
        if (!agent)
            return matching;

        const QList<QJsonArray> sent = agent->prompts();
        if (sent.isEmpty())
            return matching;

        for (const QJsonValue &block : sent.constLast()) {
            if (block.toObject().value("type").toString() == type)
                matching.append(block.toObject());
        }
        return matching;
    }

    template<typename T>
    QList<T> eventsOfType() const
    {
        QList<T> result;
        for (const Session::SessionEvent &event : events) {
            if (const auto *typed = std::get_if<T>(&event))
                result.append(*typed);
        }
        return result;
    }

    std::function<void(FakeAcpAgent *)> configure = [](FakeAcpAgent *) {};
    QHash<QString, QByteArray> storage;
    FakeAcpAgent *agent = nullptr;
    QString requestedCwd;
    QList<Session::SessionEvent> events;
    Acp::AcpChatBackend backend;
};

} // namespace

namespace {

QJsonObject writeToolCall()
{
    return QJsonObject{
        {"toolCallId", QStringLiteral("call-1")},
        {"title", QStringLiteral("Write main.cpp")},
        {"kind", QStringLiteral("edit")},
        {"status", QStringLiteral("pending")}};
}

QJsonArray writePermissionOptions()
{
    return QJsonArray{
        QJsonObject{
            {"optionId", QStringLiteral("allow")},
            {"name", QStringLiteral("Allow")},
            {"kind", QStringLiteral("allow_once")}},
        QJsonObject{
            {"optionId", QStringLiteral("deny")},
            {"name", QStringLiteral("Deny")},
            {"kind", QStringLiteral("reject_once")}}};
}

} // namespace

namespace {

class FakeKnowledgeService : public Acp::AgentKnowledgeService
{
public:
    QString start() override
    {
        ++startCount;
        running = true;
        return url;
    }

    void stop() override
    {
        ++stopCount;
        running = false;
    }

    QString serverName() const override { return QStringLiteral("qodeassist"); }

    QString url = QStringLiteral("http://127.0.0.1:54321/mcp");
    int startCount = 0;
    int stopCount = 0;
    bool running = false;
};

} // namespace

void AcpChatBackendTest::testAcpBackendStreamsTextAndThinking()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setThoughtChunks({"pondering"});
        agent->setReplyChunks({"Hello", " world"});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnStarted>().size(), 1);
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);

    const auto deltas = fixture.eventsOfType<Session::TextDelta>();
    QCOMPARE(deltas.size(), 2);
    QCOMPARE(deltas.at(0).text, QString("Hello"));
    QCOMPARE(deltas.at(1).text, QString(" world"));

    const auto thinking = fixture.eventsOfType<Session::ThinkingReceived>();
    QCOMPARE(thinking.size(), 1);
    QCOMPARE(thinking.at(0).text, QString("pondering"));

    const QString turnId = fixture.eventsOfType<Session::TurnStarted>().at(0).turnId;
    QVERIFY(!turnId.isEmpty());
    QCOMPARE(deltas.at(0).turnId, turnId);
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().at(0).turnId, turnId);

    QCOMPARE(fixture.agent->prompts().size(), 1);
    QCOMPARE(fixture.agent->prompts().at(0).size(), 1);
    QCOMPARE(
        fixture.agent->prompts().at(0).at(0).toObject().value("text").toString(), QString("hi"));
}

void AcpChatBackendTest::testAcpBackendStartsSessionInWorkingDirectory()
{
    AcpBackendFixture fixture;
    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.requestedCwd, Acp::agentWorkingDirectory());
    QCOMPARE(fixture.agent->newSessionCwd(), Acp::agentWorkingDirectory());
    QVERIFY(!fixture.agent->newSessionCwd().isEmpty());
    QCOMPARE(fixture.agent->clientInfo().value("name").toString(), QString("QodeAssistTest"));
}

void AcpChatBackendTest::testAcpBackendAuthenticatesWhenTheAgentAsksForIt()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setRequireAuthentication(true); };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
    QCOMPARE(fixture.agent->authMethodUsed(), QString("login"));
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 2);
}

void AcpChatBackendTest::testAcpBackendReportsPromptFailure()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setPromptFailure("agent exploded"); };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
    QVERIFY(fixture.eventsOfType<Session::TurnFailed>().at(0).error.contains("agent exploded"));
    QCOMPARE(
        fixture.eventsOfType<Session::TurnFailed>().at(0).turnId,
        fixture.eventsOfType<Session::TurnStarted>().at(0).turnId);
}

void AcpChatBackendTest::testAcpBackendCancelInterruptsTheTurn()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setHangOnPrompt(true);
        agent->setReplyChunks({"partial"});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TextDelta>().size(), 1);

    fixture.backend.cancel();

    QTRY_VERIFY(fixture.agent->wasCancelled());
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
}

void AcpChatBackendTest::testAcpBackendSendsAttachmentsAsEmbeddedResources()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };
    fixture.storage.insert("notes_ab12.txt", QByteArray("remember the milk"));

    fixture.sendBlocks(
        {Session::TextBlock{"summarise this"},
         Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto resources = fixture.promptBlocksOfType("resource");
    QCOMPARE(resources.size(), 1);

    const QJsonObject resource = resources.at(0).value("resource").toObject();
    QCOMPARE(resource.value("text").toString(), QString("remember the milk"));
    QCOMPARE(resource.value("mimeType").toString(), QString("text/plain"));
    QVERIFY(resource.value("uri").toString().endsWith("notes.txt"));

    QCOMPARE(fixture.promptBlocksOfType("text").size(), 1);
}

void AcpChatBackendTest::testAcpBackendInlinesAttachmentsWithoutEmbeddedContextSupport()
{
    AcpBackendFixture fixture;
    fixture.storage.insert("notes_ab12.txt", QByteArray("remember the milk"));

    fixture.sendBlocks(
        {Session::TextBlock{"summarise this"},
         Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.promptBlocksOfType("resource").size(), 0);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);

    const QString inlined = texts.at(1).value("text").toString();
    QVERIFY(inlined.contains("notes.txt"));
    QVERIFY(inlined.contains("remember the milk"));
}

void AcpChatBackendTest::testAcpBackendReportsAttachmentsItCannotLoad()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };

    fixture.sendBlocks(
        {Session::TextBlock{"summarise this"},
         Session::AttachmentBlock{"notes.txt", "missing.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.promptBlocksOfType("resource").size(), 0);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);
    QVERIFY(texts.at(1).value("text").toString().contains("notes.txt"));
}

void AcpChatBackendTest::testAcpBackendSendsImagesWhenTheAgentAcceptsThem()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsImages(true); };
    fixture.storage.insert("shot_cd34.png", QByteArray("\x89PNG-bytes"));

    fixture.sendBlocks(
        {Session::TextBlock{"what is this"},
         Session::ImageBlock{"shot.png", "shot_cd34.png", "image/png"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto images = fixture.promptBlocksOfType("image");
    QCOMPARE(images.size(), 1);
    QCOMPARE(images.at(0).value("mimeType").toString(), QString("image/png"));
    QCOMPARE(
        images.at(0).value("data").toString(),
        QString::fromLatin1(QByteArray("\x89PNG-bytes").toBase64()));
}

void AcpChatBackendTest::testAcpBackendNamesImagesTheAgentCannotAccept()
{
    AcpBackendFixture fixture;
    fixture.storage.insert("shot_cd34.png", QByteArray("\x89PNG-bytes"));

    fixture.sendBlocks(
        {Session::TextBlock{"what is this"},
         Session::ImageBlock{"shot.png", "shot_cd34.png", "image/png"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.promptBlocksOfType("image").size(), 0);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);
    QVERIFY(texts.at(1).value("text").toString().contains("shot.png"));
}

void AcpChatBackendTest::testAcpBackendFencesAttachmentsThatContainFences()
{
    AcpBackendFixture fixture;
    fixture.storage.insert("readme_ab12.md", QByteArray("intro\n```\ncode\n```\noutro"));

    fixture.sendBlocks(
        {Session::TextBlock{"read it"}, Session::AttachmentBlock{"readme.md", "readme_ab12.md"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);

    const QString inlined = texts.at(1).value("text").toString();
    QVERIFY(inlined.startsWith("File: readme.md\n````\n"));
    QVERIFY(inlined.endsWith("\n````"));
    QVERIFY(inlined.contains("```\ncode\n```"));
}

void AcpChatBackendTest::testAcpBackendTruncatesOversizedAttachments()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };

    const QByteArray huge(700 * 1024, 'x');
    fixture.storage.insert("dump_ab12.log", huge);

    fixture.sendBlocks(
        {Session::TextBlock{"summarise"}, Session::AttachmentBlock{"dump.log", "dump_ab12.log"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto resources = fixture.promptBlocksOfType("resource");
    QCOMPARE(resources.size(), 1);

    const QString text = resources.at(0).value("resource").toObject().value("text").toString();
    QVERIFY(text.size() < huge.size());
    QVERIFY(text.contains("truncated by QodeAssist"));
}

void AcpChatBackendTest::testAcpBackendRejectsAnEmptyTurn()
{
    AcpBackendFixture fixture;

    fixture.sendBlocks({Session::TextBlock{""}});

    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);
    QCOMPARE(fixture.eventsOfType<Session::TurnStarted>().size(), 0);
    QVERIFY(fixture.agent == nullptr);
}

void AcpChatBackendTest::testAcpBackendSendsAttachmentsWithoutAnyMessageText()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };
    fixture.storage.insert("notes_ab12.txt", QByteArray("remember the milk"));

    fixture.sendBlocks(
        {Session::TextBlock{""}, Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
    QCOMPARE(fixture.promptBlocksOfType("resource").size(), 1);
}

void AcpChatBackendTest::testAcpBackendRaisesAgentPermissionRequests()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const Session::PermissionRequested request
        = fixture.eventsOfType<Session::PermissionRequested>().at(0);
    QCOMPARE(request.turnId, fixture.eventsOfType<Session::TurnStarted>().at(0).turnId);
    QVERIFY(!request.requestId.isEmpty());
    QCOMPARE(request.toolCallId, QString("call-1"));
    QCOMPARE(request.title, QString("Write main.cpp"));
    QCOMPARE(request.toolKind, QString("edit"));
    QCOMPARE(request.options.size(), 2);
    QCOMPARE(request.options.at(0), (Session::PermissionOption{"allow", "Allow", "allow_once"}));
    QCOMPARE(request.options.at(1), (Session::PermissionOption{"deny", "Deny", "reject_once"}));

    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
    QVERIFY(fixture.agent->permissionOutcomes().isEmpty());
}

void AcpChatBackendTest::testAcpBackendSendsThePermissionAnswerToTheAgent()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const QString requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;
    fixture.backend.respondPermission(requestId, "allow");

    QTRY_COMPARE(fixture.agent->permissionOutcomes().size(), 1);
    QCOMPARE(
        fixture.agent->permissionOutcomes().at(0).value("outcome").toString(), QString("selected"));
    QCOMPARE(fixture.agent->permissionOutcomes().at(0).value("optionId").toString(), QString("allow"));

    const auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QCOMPARE(resolutions.at(0).requestId, requestId);
    QCOMPARE(resolutions.at(0).optionId, QString("allow"));
    QVERIFY(!resolutions.at(0).cancelled);

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
}

void AcpChatBackendTest::testAcpBackendCancelsOutstandingPermissionRequests()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const QString requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;
    fixture.backend.cancel();

    QTRY_COMPARE(fixture.agent->permissionOutcomes().size(), 1);
    QCOMPARE(
        fixture.agent->permissionOutcomes().at(0).value("outcome").toString(), QString("cancelled"));

    const auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QCOMPARE(resolutions.at(0).requestId, requestId);
    QVERIFY(resolutions.at(0).cancelled);
    QVERIFY(resolutions.at(0).optionId.isEmpty());

    QTRY_VERIFY(fixture.agent->wasCancelled());
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
}

void AcpChatBackendTest::testAcpBackendSeedsAHandoverSummaryOnce()
{
    AcpBackendFixture fixture;

    fixture.backend.setHandoverSummary("Earlier the user asked about parsing.");

    fixture.send("carry on");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const QList<QJsonObject> first = fixture.promptBlocksOfType("text");
    QCOMPARE(first.size(), 2);
    QVERIFY(first.at(0).value("text").toString().contains("Earlier the user asked about parsing."));
    QVERIFY(first.at(0).value("text").toString().contains("summary"));
    QCOMPARE(first.at(1).value("text").toString(), QString("carry on"));

    fixture.send("and now this");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 2);

    const QList<QJsonObject> second = fixture.promptBlocksOfType("text");
    QCOMPARE(second.size(), 1);
    QCOMPARE(second.at(0).value("text").toString(), QString("and now this"));
}

void AcpChatBackendTest::testAcpBackendDropsAHandoverSummaryAfterACancelledTurn()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setHangOnPrompt(true); };

    fixture.backend.setHandoverSummary("Earlier context.");

    fixture.send("carry on");
    QTRY_COMPARE(fixture.agent->prompts().size(), 1);

    QVERIFY(
        fixture.agent->prompts().at(0).at(0).toObject().value("text").toString().contains(
            "Earlier context."));

    fixture.backend.cancel();

    fixture.configure = [](FakeAcpAgent *) {};
    fixture.send("actually this");

    QTRY_COMPARE(fixture.agent->prompts().size(), 2);

    const QJsonArray second = fixture.agent->prompts().at(1);
    QCOMPARE(second.size(), 1);
    QCOMPARE(second.at(0).toObject().value("text").toString(), QString("actually this"));
}

void AcpChatBackendTest::testAcpBackendDropsAHandoverSummaryWithTheConversation()
{
    AcpBackendFixture fixture;

    fixture.backend.setHandoverSummary("stale summary");
    fixture.backend.clearToolSession(QString());

    fixture.send("fresh start");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const QList<QJsonObject> texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 1);
    QCOMPARE(texts.at(0).value("text").toString(), QString("fresh start"));
}

void AcpChatBackendTest::testAcpBackendOffersKnowledgeToHttpCapableAgents()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(true); };

    fixture.send("what am I looking at");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(knowledge.startCount, 1);
    QVERIFY(knowledge.running);

    const QJsonArray offered = fixture.agent->offeredMcpServers();
    QCOMPARE(offered.size(), 1);
    QCOMPARE(offered.at(0).toObject().value("name").toString(), QString("qodeassist"));
    QCOMPARE(offered.at(0).toObject().value("type").toString(), QString("http"));
    QCOMPARE(offered.at(0).toObject().value("url").toString(), knowledge.url);
}

void AcpChatBackendTest::testAcpBackendWithholdsKnowledgeFromAgentsWithoutHttpMcp()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(false); };

    fixture.send("what am I looking at");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(knowledge.startCount, 0);
    QVERIFY(!knowledge.running);
    QVERIFY(fixture.agent->offeredMcpServers().isEmpty());
}

void AcpChatBackendTest::testAcpBackendStopsTheKnowledgeServerWithTheSession()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(true); };

    fixture.send("hello");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QVERIFY(knowledge.running);

    fixture.backend.clearToolSession(QString());

    QCOMPARE(knowledge.stopCount, 1);
    QVERIFY(!knowledge.running);
}

void AcpChatBackendTest::testAcpBackendSurvivesAKnowledgeServerThatWillNotStart()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    knowledge.url.clear();
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(true); };

    fixture.send("hello");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(knowledge.startCount, 1);
    QVERIFY(fixture.agent->offeredMcpServers().isEmpty());
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
}

void AcpChatBackendTest::testAcpBackendResumesAPersistedSession()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(true); };

    fixture.backend.resumeSession("previous-session");
    QVERIFY(fixture.backend.isResumePending());

    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.agent->loadedSessionId(), QString("previous-session"));
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 0);
    QCOMPARE(fixture.backend.acpSessionId(), QString("previous-session"));
    QVERIFY(!fixture.backend.isResumePending());
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
}

void AcpChatBackendTest::testAcpBackendReportsAnUnresumableSession()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setSupportsLoadSession(true);
        agent->setLoadSessionError("session expired");
    };

    QStringList unavailable;
    QObject::connect(
        &fixture.backend,
        &Acp::AcpChatBackend::agentSessionUnavailable,
        &fixture.backend,
        [&unavailable](const QString &reason) { unavailable.append(reason); });

    fixture.backend.resumeSession("previous-session");
    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    QCOMPARE(unavailable.size(), 1);
    QVERIFY(unavailable.at(0).contains("session expired"));
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 0);
    QVERIFY(fixture.backend.acpSessionId().isEmpty());
    QVERIFY(!fixture.backend.isResumePending());
    QVERIFY(fixture.eventsOfType<Session::TurnFailed>().at(0).error.contains("read-only"));
}

void AcpChatBackendTest::testAcpBackendRefusesToResumeWithoutAgentSupport()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(false); };

    QStringList unavailable;
    QObject::connect(
        &fixture.backend,
        &Acp::AcpChatBackend::agentSessionUnavailable,
        &fixture.backend,
        [&unavailable](const QString &reason) { unavailable.append(reason); });

    fixture.backend.resumeSession("previous-session");
    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    QCOMPARE(unavailable.size(), 1);
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/load")), 0);
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 0);
    QVERIFY(fixture.backend.acpSessionId().isEmpty());
}

void AcpChatBackendTest::testAcpBackendStartsFreshAfterAFailedResume()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setSupportsLoadSession(true);
        agent->setLoadSessionError("session expired");
    };

    fixture.backend.resumeSession("previous-session");
    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    fixture.backend.startFreshSession();
    fixture.send("start over");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 1);
    QCOMPARE(fixture.backend.acpSessionId(), QString("fake-session"));
    QCOMPARE(fixture.agent->prompts().size(), 1);
    QCOMPARE(
        fixture.agent->prompts().at(0).at(0).toObject().value("text").toString(),
        QString("start over"));
}

void AcpChatBackendTest::testAgentBindingKeepsTheResumeTargetBeforeTheFirstTurn()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(true); };

    QVERIFY(fixture.backend.bindingSessionId().isEmpty());

    fixture.backend.resumeSession("sess-from-file");
    QCOMPARE(fixture.backend.bindingSessionId(), QString("sess-from-file"));
    QVERIFY(fixture.backend.acpSessionId().isEmpty());

    fixture.send("carry on");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.backend.acpSessionId(), QString("sess-from-file"));
    QCOMPARE(fixture.backend.bindingSessionId(), QString("sess-from-file"));
}

void AcpChatBackendTest::testAcpBackendCarriesTheLiveSessionIntoAResume()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(true); };

    fixture.send("hello");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QCOMPARE(fixture.backend.acpSessionId(), QString("fake-session"));

    fixture.backend.setChatFilePath("/somewhere/else.json");
    QCOMPARE(fixture.backend.bindingSessionId(), QString("fake-session"));

    fixture.backend.clearToolSession(QString());
    QVERIFY(fixture.backend.bindingSessionId().isEmpty());
}

void AcpChatBackendTest::testAcpBackendEstablishesTheSessionOnlyOnce()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setHangOnNewSession(true); };

    fixture.send("first");
    QTRY_COMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 1);

    fixture.backend.cancel();
    fixture.send("second");
    QTest::qWait(50);

    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 1);
    QCOMPARE(fixture.agent->prompts().size(), 0);
}

void AcpChatBackendTest::testAcpBackendMapsToolCallLifecycle()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setToolCallUpdates(
            {QJsonObject{
                 {"toolCallId", QStringLiteral("call-1")},
                 {"title", QStringLiteral("Edit main.cpp")},
                 {"kind", QStringLiteral("edit")},
                 {"status", QStringLiteral("pending")}},
             QJsonObject{
                 {"toolCallId", QStringLiteral("call-1")},
                 {"status", QStringLiteral("completed")},
                 {"locations",
                  QJsonArray{QJsonObject{{"path", QStringLiteral("/p/main.cpp")}, {"line", 12}}}},
                 {"content",
                  QJsonArray{
                      QJsonObject{
                          {"type", QStringLiteral("diff")},
                          {"path", QStringLiteral("/p/main.cpp")},
                          {"oldText", QStringLiteral("int a;")},
                          {"newText", QStringLiteral("int b;")}}}}}});
    };

    fixture.send("edit it");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto updates = fixture.eventsOfType<Session::ToolCallUpdated>();
    QCOMPARE(updates.size(), 2);

    QCOMPARE(updates.at(0).toolId, QString("call-1"));
    QCOMPARE(updates.at(0).name, QString("Edit main.cpp"));
    QCOMPARE(updates.at(0).kind, QString("edit"));
    QCOMPARE(updates.at(0).status, QString("pending"));
    QVERIFY(updates.at(0).details.isEmpty());
    QVERIFY(updates.at(0).fromAgent);
    QVERIFY(updates.at(1).fromAgent);

    QCOMPARE(updates.at(1).status, QString("completed"));
    QCOMPARE(updates.at(1).name, QString("Edit main.cpp"));

    const QJsonArray locations = updates.at(1).details.value("locations").toArray();
    QCOMPARE(locations.size(), 1);
    QCOMPARE(locations.at(0).toObject().value("path").toString(), QString("/p/main.cpp"));
    QCOMPARE(locations.at(0).toObject().value("line").toInt(), 12);

    const QJsonArray diffs = updates.at(1).details.value("diffs").toArray();
    QCOMPARE(diffs.size(), 1);
    QCOMPARE(diffs.at(0).toObject().value("oldText").toString(), QString("int a;"));
    QCOMPARE(diffs.at(0).toObject().value("newText").toString(), QString("int b;"));
}

void AcpChatBackendTest::testAcpBackendMapsPlanUpdates()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPlanUpdates(
            {QJsonArray{
                 QJsonObject{
                     {"content", QStringLiteral("Read the file")},
                     {"priority", QStringLiteral("high")},
                     {"status", QStringLiteral("in_progress")}},
                 QJsonObject{
                     {"content", QStringLiteral("Apply the fix")},
                     {"priority", QStringLiteral("medium")},
                     {"status", QStringLiteral("pending")}}},
             QJsonArray{
                 QJsonObject{
                     {"content", QStringLiteral("Read the file")},
                     {"priority", QStringLiteral("high")},
                     {"status", QStringLiteral("completed")}},
                 QJsonObject{
                     {"content", QStringLiteral("Apply the fix")},
                     {"priority", QStringLiteral("medium")},
                     {"status", QStringLiteral("in_progress")}}}});
    };

    fixture.send("fix it");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto plans = fixture.eventsOfType<Session::PlanUpdated>();
    QCOMPARE(plans.size(), 2);
    QCOMPARE(plans.at(0).entries.size(), 2);
    QCOMPARE(
        plans.at(0).entries.at(0),
        (Session::PlanEntry{"Read the file", "high", "in_progress"}));
    QCOMPARE(
        plans.at(1).entries.at(0), (Session::PlanEntry{"Read the file", "high", "completed"}));
}

void AcpChatBackendTest::testAcpBackendReportsUsageFromThePromptResult()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPromptUsage(
            QJsonObject{
                {"cachedReadTokens", 23119},
                {"cachedWriteTokens", 12271},
                {"inputTokens", 2},
                {"outputTokens", 13},
                {"totalTokens", 35405}});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto usage = fixture.eventsOfType<Session::UsageReported>();
    QCOMPARE(usage.size(), 1);
    QCOMPARE(usage.at(0).usage.promptTokens, 2);
    QCOMPARE(usage.at(0).usage.completionTokens, 13);
    QCOMPARE(usage.at(0).usage.cachedPromptTokens, 23119);
    QCOMPARE(usage.at(0).usage.reasoningTokens, 0);
}

void AcpChatBackendTest::testAcpBackendIgnoresTheCumulativeContextGauge()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setContextGauge(
            QJsonObject{
                {"size", 967000},
                {"used", 35395},
                {"cost", QJsonObject{{"amount", 0.0807}, {"currency", QStringLiteral("USD")}}}});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QCOMPARE(fixture.eventsOfType<Session::UsageReported>().size(), 0);
}

void AcpChatBackendTest::testAcpBackendForwardsTheAgentSuggestedTitle()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setSuggestedTitle("  Refactor\nthe   parser  ");
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QTRY_COMPARE(fixture.eventsOfType<Session::SessionInfo>().size(), 1);
    QCOMPARE(
        fixture.eventsOfType<Session::SessionInfo>().at(0).title,
        QString("Refactor the parser"));
}

void AcpChatBackendTest::testAcpBackendDeclinesAmbiguousPermissionOptions()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(
            writeToolCall(),
            QJsonArray{
                QJsonObject{
                    {"optionId", QStringLiteral("x")},
                    {"name", QStringLiteral("Yes, and don't ask again")},
                    {"kind", QStringLiteral("allow_always")}},
                QJsonObject{
                    {"optionId", QStringLiteral("x")},
                    {"name", QStringLiteral("No")},
                    {"kind", QStringLiteral("reject_once")}}});
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionResolved>().size(), 1);

    const auto requests = fixture.eventsOfType<Session::PermissionRequested>();
    QCOMPARE(requests.size(), 1);
    QVERIFY(requests.at(0).options.isEmpty());

    QVERIFY(fixture.eventsOfType<Session::PermissionResolved>().at(0).cancelled);
    QTRY_COMPARE(fixture.agent->permissionOutcomes().size(), 1);
    QCOMPARE(
        fixture.agent->permissionOutcomes().at(0).value("outcome").toString(), QString("cancelled"));
}

void AcpChatBackendTest::testAcpBackendClampsOversizedPermissionText()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        QJsonObject toolCall = writeToolCall();
        toolCall["title"] = QString(5000, QChar('A'));
        agent->setPermissionRequest(toolCall, writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const Session::PermissionRequested request
        = fixture.eventsOfType<Session::PermissionRequested>().at(0);
    QVERIFY(request.title.size() < 5000);
    QVERIFY(request.title.endsWith(QChar(0x2026)));
    QCOMPARE(request.options.size(), 2);
}

void AcpChatBackendTest::testAcpBackendCancelsPermissionsWhenTheTurnCompletes()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
        agent->setFinishPromptWithoutWaiting(true);
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QVERIFY(resolutions.at(0).cancelled);

    const QString requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;
    QVERIFY(!fixture.backend.respondPermission(requestId, "allow"));
}

void AcpChatBackendTest::testAcpBackendCancelsPermissionRequestsWhenTheTurnFails()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    fixture.backend.clearToolSession(QString());

    QCOMPARE(fixture.eventsOfType<Session::PermissionResolved>().size(), 1);
    QVERIFY(fixture.eventsOfType<Session::PermissionResolved>().at(0).cancelled);
}

void AcpChatBackendTest::testAcpBackendTracksAgentSlashCommands()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setAvailableCommandUpdates(
            {QJsonArray{
                 QJsonObject{
                     {"name", QStringLiteral("help")},
                     {"description", QStringLiteral("Show help")},
                     {"input", QJsonObject{{"hint", QStringLiteral("topic")}}}},
                 QJsonObject{
                     {"name", QStringLiteral("review")},
                     {"description", QStringLiteral("Review\nthe   diff")}}},
             QJsonArray{
                 QJsonObject{
                     {"name", QStringLiteral("help")},
                     {"description", QStringLiteral("Show help")}}}});
    };

    int changes = 0;
    QObject::connect(
        &fixture.backend,
        &Acp::AcpChatBackend::availableCommandsChanged,
        &fixture.backend,
        [&changes] { ++changes; });

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QTRY_COMPARE(fixture.backend.availableCommands().size(), 1);
    QCOMPARE(fixture.backend.availableCommands().at(0).name, QString("help"));
    QCOMPARE(changes, 2);

    fixture.backend.clearToolSession("chat.json");
    QCOMPARE(fixture.backend.availableCommands().size(), 0);
    QCOMPARE(changes, 3);
}

void AcpChatBackendTest::testEarlyAgentCommandsSurviveEstablishment()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setCommandsBeforeSessionReply(
            QJsonArray{QJsonObject{
                {"name", QStringLiteral("help")},
                {"description", QStringLiteral("Show help")}}});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QCOMPARE(fixture.backend.availableCommands().size(), 1);
    QCOMPARE(fixture.backend.availableCommands().at(0).name, QString("help"));
}

void AcpChatBackendTest::testAgentCommandInvocationSendsPlainPrompt()
{
    AcpBackendFixture fixture;

    fixture.send("/review src/main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QCOMPARE(fixture.agent->prompts().size(), 1);

    const QJsonArray prompt = fixture.agent->prompts().at(0);
    QCOMPARE(prompt.size(), 1);
    QCOMPARE(prompt.at(0).toObject().value("type").toString(), QString("text"));
    QCOMPARE(
        prompt.at(0).toObject().value("text").toString(), QString("/review src/main.cpp"));
}

} // namespace QodeAssist
