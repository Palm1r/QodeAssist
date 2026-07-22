// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SessionPermissionsTest.hpp"

#include <QSignalSpy>
#include <QTest>

#include "ChatView/ChatModel.hpp"
#include "SessionTestSupport.hpp"
#include "session/HistoryProjection.hpp"
#include "session/HistorySerializer.hpp"
#include "session/PermissionRequest.hpp"
#include "session/Session.hpp"

namespace QodeAssist {

void SessionPermissionsTest::testSessionRendersPermissionRequestWithItsOptions()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    QCOMPARE(session.rows().size(), 2);
    QCOMPARE(session.rows().at(1).kind, Session::RowKind::Permission);
    QCOMPARE(session.rows().at(1).id, QString("p1"));

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.requestId, QString("p1"));
    QCOMPARE(block.toolCallId, QString("tool-p1"));
    QCOMPARE(block.title, QString("Edit main.cpp"));
    QCOMPARE(block.toolKind, QString("edit"));
    QCOMPARE(block.options, editPermissionOptions());
    QCOMPARE(block.status, Session::PermissionStatus::Pending);
    QVERIFY(!block.automatic);
    QVERIFY(session.isPermissionPending("p1"));

    QVERIFY(backend.permissionAnswers().isEmpty());
}

void SessionPermissionsTest::testSessionForwardsPermissionAnswerToTheBackend()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("p1", "opt-no");

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(backend.permissionAnswers().at(0).first, QString("p1"));
    QCOMPARE(backend.permissionAnswers().at(0).second, QString("opt-no"));

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.status, Session::PermissionStatus::Answered);
    QCOMPARE(block.selectedOptionId, QString("opt-no"));
    QVERIFY(!block.automatic);
    QVERIFY(!session.isPermissionPending("p1"));
}

void SessionPermissionsTest::testSessionAllowAlwaysSuppressesRepeatPromptsOfTheSameKind()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit them"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("p1", "opt-always");

    backend.script({editPermissionRequest("r1", "p2")});

    QCOMPARE(backend.permissionAnswers().size(), 2);
    QCOMPARE(backend.permissionAnswers().at(1).first, QString("p2"));
    QCOMPARE(backend.permissionAnswers().at(1).second, QString("opt-once"));

    const Session::PermissionBlock block = permissionRowAt(session, 2);
    QCOMPARE(block.requestId, QString("p2"));
    QCOMPARE(block.status, Session::PermissionStatus::Answered);
    QCOMPARE(block.selectedOptionId, QString("opt-once"));
    QVERIFY(block.automatic);
    QVERIFY(!session.isPermissionPending("p2"));
}

void SessionPermissionsTest::testSessionAllowAlwaysDoesNotCoverOtherToolKinds()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1", "edit")});

    session.respondPermission("p1", "opt-always");

    backend.script({editPermissionRequest("r1", "p2", "execute")});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
    QVERIFY(session.isPermissionPending("p2"));
}

void SessionPermissionsTest::testSessionNeverAutoAnswersUnclassifiedTools()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1", QString())});

    session.respondPermission("p1", "opt-always");

    backend.script({editPermissionRequest("r1", "p2", QString())});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
}

void SessionPermissionsTest::testSessionIgnoresAnswersToUnknownPermissionRequests()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("nonexistent", "opt-once");
    session.respondPermission("p1", "no-such-option");
    session.respondPermission("p1", "opt-once");
    session.respondPermission("p1", "opt-no");

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(backend.permissionAnswers().at(0).second, QString("opt-once"));
    QCOMPARE(permissionRowAt(session, 1).selectedOptionId, QString("opt-once"));
}

void SessionPermissionsTest::testSessionMarksUnansweredPermissionsCancelled()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    backend.script(
        {Session::PermissionResolved{.turnId = "r1", .requestId = "p1", .cancelled = true},
         Session::TurnFailed{"r1", "the agent went away"}});

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.status, Session::PermissionStatus::Cancelled);
    QVERIFY(block.selectedOptionId.isEmpty());
    QVERIFY(!session.isPermissionPending("p1"));

    session.respondPermission("p1", "opt-once");
    QVERIFY(backend.permissionAnswers().isEmpty());
}

void SessionPermissionsTest::testSessionRejectAlwaysSuppressesRepeatPromptsOfTheSameKind()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    Session::PermissionRequested first = editPermissionRequest("r1", "p1");
    first.options.append(
        Session::PermissionOption{"opt-never", "No, and don't ask again", "reject_always"});

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, first});

    session.respondPermission("p1", "opt-never");

    Session::PermissionRequested second = editPermissionRequest("r1", "p2");
    second.options.append(
        Session::PermissionOption{"opt-never", "No, and don't ask again", "reject_always"});
    backend.script({second});

    QCOMPARE(backend.permissionAnswers().size(), 2);
    QCOMPARE(backend.permissionAnswers().at(1).second, QString("opt-no"));

    const Session::PermissionBlock block = permissionRowAt(session, 2);
    QCOMPARE(block.status, Session::PermissionStatus::Answered);
    QCOMPARE(block.selectedOptionId, QString("opt-no"));
    QVERIFY(block.automatic);
}

void SessionPermissionsTest::testSessionAutoAnswerFallsBackToTheAlwaysOption()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});
    session.respondPermission("p1", "opt-always");

    Session::PermissionRequested withoutOnce = editPermissionRequest("r1", "p2");
    withoutOnce.options = {
        Session::PermissionOption{"opt-always", "Yes, always", "allow_always"},
        Session::PermissionOption{"opt-no", "No", "reject_once"}};
    backend.script({withoutOnce});

    QCOMPARE(backend.permissionAnswers().size(), 2);
    QCOMPARE(backend.permissionAnswers().at(1).second, QString("opt-always"));
}

void SessionPermissionsTest::testSessionDoesNotAutoAnswerWithoutAMatchingOption()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});
    session.respondPermission("p1", "opt-always");

    Session::PermissionRequested rejectOnly = editPermissionRequest("r1", "p2");
    rejectOnly.options = {Session::PermissionOption{"opt-no", "No", "reject_once"}};
    backend.script({rejectOnly});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
    QVERIFY(session.isPermissionPending("p2"));
}

void SessionPermissionsTest::testSessionAllowAlwaysDoesNotSurviveAnotherConversation()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});
    session.respondPermission("p1", "opt-always");
    QCOMPARE(backend.permissionAnswers().size(), 1);

    session.setHistory(Session::ConversationHistory{});

    session.sendTurn({Session::TextBlock{"work again"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r2"}, editPermissionRequest("r2", "p2")});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 1).status, Session::PermissionStatus::Pending);
}

void SessionPermissionsTest::testSessionAnswersTheLiveRequestWhenAnOldIdIsReused()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "old";
    assistant.blocks
        = {Session::PermissionBlock{
            .requestId = "p1",
            .toolCallId = "tool-old",
            .title = "Run make",
            .toolKind = "execute",
            .options = editPermissionOptions(),
            .status = Session::PermissionStatus::Cancelled}};

    Session::ConversationHistory history;
    history.append(assistant);

    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);
    session.setHistory(history);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1", "edit")});

    session.respondPermission("p1", "opt-always");

    QCOMPARE(backend.permissionAnswers().size(), 1);

    const Session::PermissionBlock stale = permissionRowAt(session, 0);
    QCOMPARE(stale.status, Session::PermissionStatus::Cancelled);
    QVERIFY(stale.selectedOptionId.isEmpty());

    const Session::PermissionBlock live = permissionRowAt(session, 2);
    QCOMPARE(live.status, Session::PermissionStatus::Answered);
    QCOMPARE(live.selectedOptionId, QString("opt-always"));

    backend.script({editPermissionRequest("r1", "p3", "edit")});
    QCOMPARE(backend.permissionAnswers().size(), 2);

    backend.script({editPermissionRequest("r1", "p4", "execute")});
    QCOMPARE(backend.permissionAnswers().size(), 2);
}

void SessionPermissionsTest::testSessionCancelsPermissionsTheBackendNeverResolved()
{
    FakeChatBackend backend;
    backend.setAutoResolvePermissions(false);

    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         editPermissionRequest("r1", "p1"),
         Session::TurnCompleted{"r1"}});

    QCOMPARE(permissionRowAt(session, 1).status, Session::PermissionStatus::Cancelled);
    QVERIFY(!session.isPermissionPending("p1"));
}

void SessionPermissionsTest::testSessionCancelsPermissionsTheBackendRefused()
{
    FakeChatBackend backend;
    backend.setAcceptPermissionAnswers(false);

    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("p1", "opt-always");

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.status, Session::PermissionStatus::Cancelled);
    QVERIFY(block.selectedOptionId.isEmpty());
    QVERIFY(!session.isPermissionPending("p1"));

    backend.script({editPermissionRequest("r1", "p2")});
    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
}

void SessionPermissionsTest::testPermissionBlockReopensAsUnanswerable()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks
        = {Session::PermissionBlock{
               .requestId = "p1",
               .toolCallId = "tool-1",
               .title = "Edit main.cpp",
               .toolKind = "edit",
               .options = editPermissionOptions(),
               .status = Session::PermissionStatus::Answered,
               .selectedOptionId = "opt-once",
               .automatic = false},
           Session::PermissionBlock{
               .requestId = "p2",
               .toolCallId = "tool-2",
               .title = "Run make",
               .toolKind = "execute",
               .options = editPermissionOptions(),
               .status = Session::PermissionStatus::Pending}};

    Session::ConversationHistory history;
    history.append(assistant);

    const QJsonObject json = Session::HistorySerializer::toJson(history);
    const auto reloaded = Session::HistorySerializer::fromJson(json);
    QVERIFY(reloaded.has_value());
    QCOMPARE(reloaded->size(), 1);
    QCOMPARE(reloaded->at(0).blocks.size(), 2);

    const auto *answered = std::get_if<Session::PermissionBlock>(&reloaded->at(0).blocks.at(0));
    QVERIFY(answered);
    QCOMPARE(*answered, std::get<Session::PermissionBlock>(assistant.blocks.at(0)));

    const auto *stale = std::get_if<Session::PermissionBlock>(&reloaded->at(0).blocks.at(1));
    QVERIFY(stale);
    QCOMPARE(stale->status, Session::PermissionStatus::Cancelled);
    QCOMPARE(stale->options, editPermissionOptions());

    const QList<Session::MessageRow> rows = Session::projectToRows(*reloaded);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).kind, Session::RowKind::Permission);

    const Session::ConversationHistory rebuilt = Session::buildFromRows(rows);
    QCOMPARE(rebuilt.size(), 1);
    QCOMPARE(rebuilt.at(0).blocks, reloaded->at(0).blocks);
}

} // namespace QodeAssist
