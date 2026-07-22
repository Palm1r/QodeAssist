// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "LlmChatBackendTest.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include <LLMQore/ToolsManager.hpp>

#include "ChatView/LlmChatBackend.hpp"
#include "FakeLlmProvider.hpp"
#include "session/ConversationHistory.hpp"
#include "session/HistoryProjection.hpp"
#include "session/Session.hpp"
#include "session/TurnLedger.hpp"

namespace QodeAssist {

namespace {

class LlmBackendFixture
{
public:
    LlmBackendFixture()
        : backend(&promptProvider)
    {
        backend.setProviderResolver([this](const QString &) { return &provider; });

        QObject::connect(
            &backend,
            &Session::ChatBackend::sessionEvent,
            &backend,
            [this](const Session::SessionEvent &event) { events.append(event); });

        Session::Message user;
        user.role = Session::MessageRole::User;
        user.id = "u1";
        user.blocks = {Session::TextBlock{"hello"}};
        history.append(user);
    }

    QString send()
    {
        Session::TurnRequest request;
        request.userBlocks = {Session::TextBlock{"hello"}};
        request.history = &history;
        backend.sendTurn(request);

        const auto started = eventsOfType<Session::TurnStarted>();
        return started.isEmpty() ? QString() : started.constLast().turnId;
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

    FakeChatPromptProvider promptProvider;
    FakeLlmProvider provider;
    Session::ConversationHistory history;
    QList<Session::SessionEvent> events;
    Chat::LlmChatBackend backend;
};

} // namespace

void LlmChatBackendTest::testTurnLedgerTracksTheActiveTurn()
{
    Session::TurnLedger ledger;

    QVERIFY(!ledger.hasActiveTurn());
    QVERIFY(!ledger.isActiveTurn(QString()));
    QVERIFY(!ledger.isActiveTurn("t1"));

    QCOMPARE(ledger.beginTurn("t1"), QString("t1"));
    QVERIFY(ledger.hasActiveTurn());
    QVERIFY(ledger.isActiveTurn("t1"));
    QVERIFY(!ledger.isActiveTurn("t2"));
    QVERIFY(!ledger.isActiveTurn(QString()));

    ledger.endTurn();
    QVERIFY(!ledger.hasActiveTurn());
    QVERIFY(!ledger.isActiveTurn("t1"));
}

void LlmChatBackendTest::testTurnLedgerDrainsPermissionsOnTurnEnd()
{
    Session::TurnLedger ledger;
    ledger.beginTurn("t1");

    QStringList resolved;
    int cancelled = 0;

    const QString first = ledger.registerPermission(
        [&resolved](const QString &optionId) { resolved.append(optionId); },
        [&cancelled] { ++cancelled; });
    const QString second = ledger.registerPermission(
        [&resolved](const QString &optionId) { resolved.append(optionId); },
        [&cancelled] { ++cancelled; });

    QVERIFY(first.startsWith("perm-"));
    QVERIFY(second.startsWith("perm-"));
    QVERIFY(first != second);
    QCOMPARE(ledger.pendingPermissionCount(), 2);

    QVERIFY(ledger.resolvePermission(first, "allow_once"));
    QCOMPARE(resolved, QStringList{"allow_once"});
    QVERIFY(!ledger.resolvePermission(first, "allow_once"));
    QVERIFY(!ledger.hasPendingPermission(first));
    QVERIFY(ledger.hasPendingPermission(second));

    const QStringList drained = ledger.endTurn();
    QCOMPARE(drained, QStringList{second});
    QCOMPARE(cancelled, 1);
    QCOMPARE(ledger.pendingPermissionCount(), 0);
    QVERIFY(!ledger.resolvePermission(second, "allow_once"));
    QCOMPARE(resolved, QStringList{"allow_once"});
}

void LlmChatBackendTest::testLlmBackendStreamsThroughAFakeClient()
{
    LlmBackendFixture fixture;

    const QString turnId = fixture.send();
    QCOMPARE(turnId, QString("fake-req-1"));
    QVERIFY(fixture.provider.fakeClient()->lastPayload.contains("model"));
    QVERIFY(fixture.provider.fakeClient()->lastPayload.contains("stream"));

    emit fixture.provider.fakeClient()->chunkReceived(turnId, "hel");
    emit fixture.provider.fakeClient()->chunkReceived(turnId, "lo");
    emit fixture.provider.fakeClient()->requestCompleted(turnId, "hello");

    const auto deltas = fixture.eventsOfType<Session::TextDelta>();
    QCOMPARE(deltas.size(), 2);
    QCOMPARE(deltas.at(0).text, QString("hel"));
    QCOMPARE(deltas.at(1).text, QString("lo"));
    QCOMPARE(deltas.at(0).turnId, turnId);

    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().at(0).turnId, turnId);
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
}

void LlmChatBackendTest::testRenderHistoryKeepsToolExchangePairs()
{
    LlmBackendFixture fixture;

    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r0";
    assistant.blocks
        = {Session::ToolCallBlock{
               "t1",
               "read_file",
               QJsonObject{{"path", "main.cpp"}},
               "int main() {}",
               {},
               "completed"},
           Session::ToolCallBlock{
               "call-9",
               "Edit main.cpp",
               QJsonObject{},
               "done",
               "edit",
               "completed",
               QJsonObject{},
               true},
           Session::TextBlock{"the answer"}};
    fixture.history.append(assistant);

    fixture.send();

    QVERIFY(fixture.provider.lastContext.history.has_value());
    const QVector<LLMCore::Message> &messages = *fixture.provider.lastContext.history;

    QCOMPARE(messages.size(), 4);
    QCOMPARE(messages.at(0).role, QString("user"));
    QCOMPARE(messages.at(1).role, QString("assistant"));
    QCOMPARE(messages.at(1).toolCalls.size(), 1);
    QCOMPARE(messages.at(1).toolCalls.at(0).id, QString("t1"));
    QCOMPARE(messages.at(1).toolCalls.at(0).name, QString("read_file"));
    QCOMPARE(
        messages.at(1).toolCalls.at(0).arguments, (QJsonObject{{"path", "main.cpp"}}));
    QCOMPARE(messages.at(2).role, QString("tool"));
    QCOMPARE(messages.at(2).toolCallId, QString("t1"));
    QCOMPARE(messages.at(2).content, QString("int main() {}"));
    QCOMPARE(messages.at(3).role, QString("assistant"));
    QCOMPARE(messages.at(3).content, QString("the answer"));
}

void LlmChatBackendTest::testLlmBackendIgnoresEventsFromOtherRequests()
{
    LlmBackendFixture fixture;

    const QString turnId = fixture.send();
    QVERIFY(!turnId.isEmpty());

    emit fixture.provider.fakeClient()->chunkReceived("stale-req", "ignored");
    QCOMPARE(fixture.eventsOfType<Session::TextDelta>().size(), 0);

    fixture.backend.cancel();

    emit fixture.provider.fakeClient()->chunkReceived(turnId, "late");
    QCOMPARE(fixture.eventsOfType<Session::TextDelta>().size(), 0);
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
}

void LlmChatBackendTest::testLlmBackendPermissionRoundTrip()
{
    LlmBackendFixture fixture;
    auto *tool = new FakeMutatingTool(&fixture.backend);
    fixture.provider.toolsManager()->addTool(tool);

    const QString turnId = fixture.send();

    fixture.provider.toolsManager()->executeToolCall(turnId, "t1", "fake_write", {});

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);
    const Session::PermissionRequested request
        = fixture.eventsOfType<Session::PermissionRequested>().at(0);
    QCOMPARE(request.turnId, turnId);
    QVERIFY(request.requestId.startsWith("perm-"));
    QCOMPARE(request.toolKind, QString("fake_write"));
    QCOMPARE(request.options.size(), 3);

    QVERIFY(fixture.backend.respondPermission(request.requestId, "allow_once"));
    QTRY_COMPARE(tool->executions, 1);

    auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QCOMPARE(resolutions.at(0).requestId, request.requestId);
    QCOMPARE(resolutions.at(0).optionId, QString("allow_once"));
    QVERIFY(!resolutions.at(0).cancelled);

    fixture.provider.toolsManager()->executeToolCall(turnId, "t2", "fake_write", {});
    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 2);
    const QString second = fixture.eventsOfType<Session::PermissionRequested>().at(1).requestId;
    QVERIFY(second != request.requestId);

    QVERIFY(fixture.backend.respondPermission(second, "reject_once"));
    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionResolved>().size(), 2);
    QCOMPARE(tool->executions, 1);

    QTRY_VERIFY(
        !fixture.eventsOfType<Session::ToolCallUpdated>().isEmpty()
        && fixture.eventsOfType<Session::ToolCallUpdated>().constLast().toolId == QString("t2"));
    const Session::ToolCallUpdated declined
        = fixture.eventsOfType<Session::ToolCallUpdated>().constLast();
    QCOMPARE(declined.toolId, QString("t2"));
    QCOMPARE(declined.status, QString("failed"));
    QVERIFY(declined.result.startsWith("Error: "));
}

void LlmChatBackendTest::testLlmBackendDrainsPermissionsWhenTheTurnEnds()
{
    LlmBackendFixture fixture;
    auto *tool = new FakeMutatingTool(&fixture.backend);
    fixture.provider.toolsManager()->addTool(tool);

    QString turnId = fixture.send();
    fixture.provider.toolsManager()->executeToolCall(turnId, "t1", "fake_write", {});
    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);
    QString requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;

    emit fixture.provider.fakeClient()->requestCompleted(turnId, "done");

    auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QCOMPARE(resolutions.at(0).requestId, requestId);
    QVERIFY(resolutions.at(0).cancelled);
    QVERIFY(!fixture.backend.respondPermission(requestId, "allow_once"));
    QCOMPARE(tool->executions, 0);

    fixture.events.clear();
    turnId = fixture.send();
    fixture.provider.toolsManager()->executeToolCall(turnId, "t2", "fake_write", {});
    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);
    requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;

    emit fixture.provider.fakeClient()->requestFailed(turnId, "boom");

    resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QVERIFY(resolutions.at(0).cancelled);
    QVERIFY(!fixture.backend.respondPermission(requestId, "allow_once"));
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    fixture.events.clear();
    turnId = fixture.send();
    fixture.provider.toolsManager()->executeToolCall(turnId, "t3", "fake_write", {});
    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);
    requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;

    fixture.backend.cancel();

    resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QVERIFY(resolutions.at(0).cancelled);
    QVERIFY(!fixture.backend.respondPermission(requestId, "allow_once"));
    QCOMPARE(tool->executions, 0);
}

} // namespace QodeAssist
