// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SessionTest.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "ChatView/ChatModel.hpp"
#include "SessionTestSupport.hpp"
#include "context/FileEditManager.hpp"
#include "session/FileEditPayload.hpp"
#include "session/HistoryProjection.hpp"
#include "session/HistorySerializer.hpp"
#include "session/Session.hpp"

namespace QodeAssist {

void SessionTest::testSessionAppendsUserTurnBeforeSending()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn(
        {Session::TextBlock{"explain this"}, Session::AttachmentBlock{"notes.txt", "notes_ab.txt"}},
        std::nullopt);

    QCOMPARE(backend.turnCount(), 1);
    QCOMPARE(backend.historySizeAtSend(), qsizetype(1));
    QCOMPARE(backend.lastUserBlocks().size(), 2);
    QVERIFY(!backend.lastContext().has_value());

    QCOMPARE(session.history().size(), 1);
    QCOMPARE(session.history().at(0).role, Session::MessageRole::User);
    QCOMPARE(session.rows().size(), 1);
    QCOMPARE(session.rows().at(0).content, QString("explain this"));
    QCOMPARE(session.rows().at(0).attachments.size(), 1);
}

void SessionTest::testSessionStreamsTextThinkingAndTools()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"explain this"}}, std::nullopt);
    backend.script(scriptedTurn("r1"));

    QCOMPARE(session.history().size(), 2);

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.role, Session::MessageRole::Assistant);
    QCOMPARE(assistant.id, QString("r1"));
    QCOMPARE(assistant.usage, (Session::Usage{120, 40, 80, 12}));
    QCOMPARE(assistant.blocks.size(), 4);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{Session::ThinkingBlock{"weighing options", "sig-1", false}}));
    QCOMPARE(assistant.blocks.at(1), (Session::ContentBlock{Session::TextBlock{"let me check"}}));
    QCOMPARE(
        assistant.blocks.at(2),
        (Session::ContentBlock{Session::ToolCallBlock{
            "t1",
            "read_file",
            QJsonObject{{"path", "main.cpp"}},
            "int main() {}",
            {},
            "completed"}}));
    QCOMPARE(
        assistant.blocks.at(3), (Session::ContentBlock{Session::TextBlock{"here is the answer"}}));
}

void SessionTest::testSessionTrimsStreamedText()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::TextDelta{"r1", "\n\n"},
         Session::TextDelta{"r1", "  here is"},
         Session::TextDelta{"r1", " the answer  \n"}});

    QCOMPARE(session.history().at(1).blocks.size(), 1);
    QCOMPARE(
        session.history().at(1).blocks.at(0),
        (Session::ContentBlock{Session::TextBlock{"here is the answer"}}));
}

void SessionTest::testSessionAbandoningTheTurnCancelsTheBackend()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, Session::TextDelta{"r1", "partial"}});

    const int cancelsBeforeTruncate = backend.cancelCount();
    session.truncateRows(0);
    QCOMPARE(backend.cancelCount(), cancelsBeforeTruncate + 1);

    const int cancelsBeforeLoad = backend.cancelCount();
    session.setHistory(Session::ConversationHistory{});
    QCOMPARE(backend.cancelCount(), cancelsBeforeLoad + 1);
}

void SessionTest::testSwitchingBackendsCancelsTheActiveTurn()
{
    FakeChatBackend first;
    FakeChatBackend second;
    Session::Session session;
    session.setBackend(&first);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    first.script({Session::TurnStarted{"r1"}, Session::TextDelta{"r1", "partial"}});

    const int cancelsBeforeSwap = first.cancelCount();
    session.setBackend(&second);
    QCOMPARE(first.cancelCount(), cancelsBeforeSwap + 1);

    const qsizetype rowsAfterSwap = session.rows().size();
    first.script({Session::TextDelta{"r1", " stale"}});
    QCOMPARE(session.rows().size(), rowsAfterSwap);
    QCOMPARE(session.rows().at(1).content, QString("partial"));
}

void SessionTest::testSessionForwardsTitleFromTheEventStream()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    QStringList titles;
    connect(
        &session,
        &Session::Session::sessionInfoReceived,
        this,
        [&titles](const QString &title) { titles.append(title); });

    session.sendTurn({Session::TextBlock{"hi"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::TextDelta{"r1", "hello"},
         Session::SessionInfo{.title = "Working title"},
         Session::TurnCompleted{"r1"},
         Session::SessionInfo{.title = "Chat about parsers"}});

    QCOMPARE(titles, (QStringList{"Working title", "Chat about parsers"}));

    backend.script({Session::SessionInfo{.title = QString()}});
    QCOMPARE(titles.size(), 2);
}

void SessionTest::testSessionProjectionMatchesHistory()
{
    Chat::ChatModel model;
    FakeChatBackend backend;
    Session::Session session;
    attachModelToSession(session, model);
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"explain this"}}, std::nullopt);
    backend.script(scriptedTurn("r1"));

    const QList<Session::MessageRow> expected = Session::projectToRows(session.history());
    QCOMPARE(session.rows(), expected);
    QCOMPARE(model.rowCount(), int(expected.size()));

    for (int row = 0; row < expected.size(); ++row) {
        QCOMPARE(
            model.data(model.index(row), Chat::ChatModel::Content).toString(),
            expected.at(row).content);
    }

    QCOMPARE(model.sessionPromptTokens(), 120);
    QCOMPARE(model.sessionCompletionTokens(), 40);
}

void SessionTest::testSessionAccumulatesStreamedThinking()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ThinkingReceived{"r1", "step", QString(), false},
         Session::ThinkingReceived{"r1", "step one", QString(), false},
         Session::ThinkingReceived{"r1", "step one and two", QString(), false},
         Session::TextDelta{"r1", "done"}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{Session::ThinkingBlock{"step one and two", QString(), false}}));
    QCOMPARE(session.rows().size(), 3);
}

void SessionTest::testSessionKeepsThinkingAfterToolContinuation()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ThinkingReceived{"r1", "first thought", "sig-a", false},
         llmToolStarted("r1", "t1", "read_file"),
         llmToolCompleted("r1", "t1", "read_file", "contents"),
         Session::ThinkingReceived{"r1", "second thought", "sig-b", false},
         Session::TextDelta{"r1", "done"}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 4);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{Session::ThinkingBlock{"first thought", "sig-a", false}}));
    QCOMPARE(
        assistant.blocks.at(2),
        (Session::ContentBlock{Session::ThinkingBlock{"second thought", "sig-b", false}}));
}

void SessionTest::testSessionDropsPreToolTextWhenAsked()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::TextDelta{"r1", "leaked tool preamble"},
         llmToolStarted("r1", "t1", "read_file", {}, true),
         llmToolCompleted("r1", "t1", "read_file", "contents"),
         Session::TextDelta{"r1", "the answer"}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{Session::ToolCallBlock{
            "t1", "read_file", QJsonObject{}, "contents", {}, "completed"}}));
    QCOMPARE(assistant.blocks.at(1), (Session::ContentBlock{Session::TextBlock{"the answer"}}));
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void SessionTest::testSessionToolResultAppendsFileEditRow()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    const QString result
        = "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\",\"file\":\"main.cpp\",\"status\":\"pending\"}";

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         llmToolStarted("r1", "t1", "edit_file"),
         llmToolCompleted("r1", "t1", "edit_file", result)});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);
    QCOMPARE(assistant.blocks.at(1), (Session::ContentBlock{Session::FileEditBlock{"e1", result}}));

    QVERIFY(session.updateFileEditStatus("e1", "applied", "Successfully applied"));

    const Session::MessageRow &editRow = session.rows().last();
    QCOMPARE(editRow.kind, Session::RowKind::FileEdit);
    QVERIFY(editRow.content.contains("\"status\":\"applied\""));
    QVERIFY(editRow.content.contains("\"status_message\":\"Successfully applied\""));
}

void SessionTest::testSessionRepeatedToolResultKeepsOneFileEdit()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    const QString result
        = "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\",\"file\":\"main.cpp\",\"status\":\"pending\"}";

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         llmToolStarted("r1", "t1", "edit_file"),
         llmToolCompleted("r1", "t1", "edit_file", result),
         llmToolCompleted("r1", "t1", "edit_file", result)});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);

    qsizetype editBlocks = 0;
    for (const Session::ContentBlock &block : assistant.blocks) {
        if (std::holds_alternative<Session::FileEditBlock>(block))
            ++editBlocks;
    }
    QCOMPARE(editBlocks, qsizetype(1));
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void SessionTest::testRunningToolStatusInterruptsOnTurnEnd()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, llmToolStarted("r1", "t1", "read_file")});
    QCOMPARE(session.rows().at(1).toolStatus, QString("in_progress"));

    session.cancel();

    QCOMPARE(session.rows().at(1).toolStatus, QString("interrupted"));

    const auto *tool
        = std::get_if<Session::ToolCallBlock>(&session.history().at(1).blocks.at(0));
    QVERIFY(tool);
    QCOMPARE(tool->status, QString("interrupted"));
}

void SessionTest::testRunningToolStatusInterruptsOnLoad()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks
        = {Session::ToolCallBlock{
            "call-1", "Edit main.cpp", QJsonObject{}, {}, "edit", "in_progress", {}, true}};

    Session::ConversationHistory history;
    history.append(assistant);

    const auto reloaded
        = Session::HistorySerializer::fromJson(Session::HistorySerializer::toJson(history));
    QVERIFY(reloaded.has_value());

    const auto *tool = std::get_if<Session::ToolCallBlock>(&reloaded->at(0).blocks.at(0));
    QVERIFY(tool);
    QCOMPARE(tool->status, QString("interrupted"));

    const auto rebuilt = Session::buildFromRows(Session::projectToRows(history));
    const auto *rowTool = std::get_if<Session::ToolCallBlock>(&rebuilt.at(0).blocks.at(0));
    QVERIFY(rowTool);
    QCOMPARE(rowTool->status, QString("interrupted"));

    QCOMPARE(Session::restoredToolStatus("completed"), QString("completed"));
    QCOMPARE(Session::restoredToolStatus("failed"), QString("failed"));
    QCOMPARE(Session::restoredToolStatus(QString()), QString());
}

void SessionTest::testAgentDiffBecomesAppliedFileEdit()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    struct Recorded
    {
        QString turnId;
        QString editId;
        QString filePath;
        QString oldContent;
        QString newContent;
    };
    QList<Recorded> recorded;
    connect(
        &session,
        &Session::Session::agentFileEditRecorded,
        this,
        [&recorded](
            const QString &turnId,
            const QString &editId,
            const QString &filePath,
            const QString &oldContent,
            const QString &newContent) {
            recorded.append({turnId, editId, filePath, oldContent, newContent});
        });

    const QJsonObject details{
        {"diffs",
         QJsonArray{QJsonObject{
             {"path", "/proj/main.cpp"}, {"oldText", "int a;"}, {"newText", "int b;"}}}}};

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit main.cpp",
             .kind = "edit",
             .status = "in_progress",
             .fromAgent = true},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit main.cpp",
             .kind = "edit",
             .status = "completed",
             .details = details,
             .fromAgent = true},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit main.cpp",
             .kind = "edit",
             .status = "completed",
             .details = details,
             .fromAgent = true}});

    QCOMPARE(recorded.size(), 1);
    QCOMPARE(recorded.at(0).turnId, QString("r1"));
    QCOMPARE(recorded.at(0).editId, QString("agent-call-1-0"));
    QCOMPARE(recorded.at(0).filePath, QString("/proj/main.cpp"));
    QCOMPARE(recorded.at(0).oldContent, QString("int a;"));
    QCOMPARE(recorded.at(0).newContent, QString("int b;"));

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);

    const auto *edit = std::get_if<Session::FileEditBlock>(&assistant.blocks.at(1));
    QVERIFY(edit);
    QCOMPARE(edit->id, QString("agent-call-1-0"));

    const auto payload = Session::parseFileEditPayload(edit->payload);
    QVERIFY(payload.has_value());
    QCOMPARE(payload->value("status").toString(), QString("applied"));
    QCOMPARE(payload->value("file").toString(), QString("/proj/main.cpp"));
    QCOMPARE(payload->value("old_content").toString(), QString("int a;"));
    QCOMPARE(payload->value("new_content").toString(), QString("int b;"));

    const Session::MessageRow &editRow = session.rows().last();
    QCOMPARE(editRow.kind, Session::RowKind::FileEdit);
    QVERIFY(editRow.content.contains("\"status\":\"applied\""));
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void SessionTest::testAgentDiffSkipsUnrevertableRegistrations()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    int recorded = 0;
    connect(
        &session,
        &Session::Session::agentFileEditRecorded,
        this,
        [&recorded](
            const QString &, const QString &, const QString &, const QString &, const QString &) {
            ++recorded;
        });

    const QJsonObject details{
        {"diffs",
         QJsonArray{
             QJsonObject{{"path", "/proj/deleted.cpp"}, {"oldText", "gone"}, {"newText", ""}},
             QJsonObject{
                 {"path", "/proj/big.cpp"},
                 {"oldText", "int a;"},
                 {"newText", "int b;"},
                 {"truncated", true}}}}};

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit files",
             .kind = "edit",
             .status = "completed",
             .details = details,
             .fromAgent = true}});

    QCOMPARE(recorded, 0);

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 3);
    QVERIFY(std::get_if<Session::FileEditBlock>(&assistant.blocks.at(1)));
    QVERIFY(std::get_if<Session::FileEditBlock>(&assistant.blocks.at(2)));
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void SessionTest::testAgentFileEditRevertRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("edited.cpp");
    const QString oldContent = "int a;\n";
    const QString newContent = "int b;\n";

    {
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(newContent.toUtf8());
    }

    const QString editId
        = "agent-test-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString requestId
        = "req-" + QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto &changes = Context::FileEditManager::instance();
    changes.registerAppliedFileEdit(editId, filePath, oldContent, newContent, requestId);

    const auto registered = changes.getFileEdit(editId);
    QCOMPARE(registered.status, Context::FileEditManager::Applied);
    QCOMPARE(registered.filePath, filePath);

    QVERIFY(changes.applyPendingEditsForRequest(requestId));

    QSignalSpy undoneSpy(&changes, &Context::FileEditManager::fileEditUndone);
    QVERIFY(changes.undoFileEdit(editId));
    QCOMPARE(undoneSpy.count(), 1);
    QCOMPARE(undoneSpy.at(0).at(0).toString(), editId);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), oldContent);

    QCOMPARE(changes.getFileEdit(editId).status, Context::FileEditManager::Rejected);
}

void SessionTest::testSessionIgnoresEchoedFileEditMarker()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    const QString echoed
        = "Pre-compression history (/src): 1 matching message(s)\n\n[#3 assistant]\n"
          "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\",\"file\":\"main.cpp\"}";

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         llmToolStarted("r1", "t1", "read_original_history"),
         llmToolCompleted("r1", "t1", "read_original_history", echoed)});

    QCOMPARE(session.history().at(1).blocks.size(), 1);
    QCOMPARE(session.rows().size(), 2);
}

void SessionTest::testSessionTruncationKeepsBlockOrder()
{
    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks
        = {Session::TextBlock{"look at this"},
           Session::AttachmentBlock{"notes.txt", "notes_ab.txt"},
           Session::ImageBlock{"shot.png", "shot_cd.png", "image/png"}};

    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks = {Session::TextBlock{"sure"}};

    Session::ConversationHistory history;
    history.append(user);
    history.append(assistant);

    Session::Session session;
    session.setHistory(history);
    session.truncateRows(1);

    QCOMPARE(session.history().size(), 1);
    QCOMPARE(session.history().at(0).blocks, user.blocks);
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void SessionTest::testHistorySnapshotReportsUnreadableBlocks()
{
    QJsonObject block;
    block["type"] = "from_a_newer_build";

    QJsonObject message;
    message["role"] = "assistant";
    message["blocks"] = QJsonArray{block, QJsonObject{{"type", "text"}, {"text", "kept"}}};

    QJsonObject root;
    root["version"] = Session::HistorySerializer::currentVersion();
    root["messages"] = QJsonArray{message};

    int dropped = -1;
    const auto history = Session::HistorySerializer::fromJson(root, &dropped);

    QVERIFY(history.has_value());
    QCOMPARE(dropped, 1);
    QCOMPARE(history->at(0).blocks.size(), 1);
}

void SessionTest::testSessionUsageLandsOnTheTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ThinkingReceived{"r1", "hmm", "sig", false},
         Session::TextDelta{"r1", "done"},
         Session::UsageReported{"r1", Session::Usage{10, 5, 2, 1}},
         Session::TurnCompleted{"r1"}});

    QCOMPARE(session.history().at(1).usage, (Session::Usage{10, 5, 2, 1}));
    QCOMPARE(session.rows().at(1).usage, (Session::Usage{10, 5, 2, 1}));
    QCOMPARE(session.rows().at(2).usage, Session::Usage{});
}

void SessionTest::testSessionIgnoresEventsFromOtherTurns()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::TextDelta{"r1", "mine"},
         Session::TextDelta{"other", "not mine"},
         llmToolStarted("other", "t9", "read_file")});

    QCOMPARE(session.history().size(), 2);
    QCOMPARE(session.history().at(1).blocks.size(), 1);
    QCOMPARE(
        session.history().at(1).blocks.at(0), (Session::ContentBlock{Session::TextBlock{"mine"}}));
}

void SessionTest::testSessionReportsFailureWithoutStartedTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    QString reported;
    QObject::connect(
        &session, &Session::Session::turnFailed, &session, [&reported](const QString &error) {
            reported = error;
        });

    backend.script({Session::TurnFailed{QString(), "no provider configured"}});

    QCOMPARE(reported, QString("no provider configured"));
}

void SessionTest::testSessionIgnoresFailureFromAbandonedTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    int reported = 0;
    QObject::connect(&session, &Session::Session::turnFailed, &session, [&reported](const QString &) {
        ++reported;
    });

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script({Session::TurnStarted{"r1"}, Session::TextDelta{"r1", "partial"}});
    session.truncateRows(0);

    backend.script({Session::TurnFailed{"r1", "stale timeout"}});

    QCOMPARE(reported, 0);
}

void SessionTest::testSessionTruncateRowsRewritesHistory()
{
    Chat::ChatModel model;
    FakeChatBackend backend;
    Session::Session session;
    attachModelToSession(session, model);
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"explain this"}}, std::nullopt);
    backend.script(scriptedTurn("r1"));

    const qsizetype before = session.rows().size();
    QVERIFY(before > 1);

    session.truncateRows(1);

    QCOMPARE(session.rows().size(), qsizetype(1));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(session.history().size(), 1);
    QCOMPARE(session.history().at(0).role, Session::MessageRole::User);
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void SessionTest::testSessionUpsertsAgentToolCalls()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit main.cpp",
             .kind = "edit",
             .status = "pending",
             .fromAgent = true},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit main.cpp",
             .kind = "edit",
             .status = "completed",
             .result = "done",
             .details = QJsonObject{{"locations", QJsonArray{QJsonObject{{"path", "/p/main.cpp"}}}}},
             .fromAgent = true}});

    QCOMPARE(session.history().size(), 2);

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 1);

    const auto *tool = std::get_if<Session::ToolCallBlock>(&assistant.blocks.at(0));
    QVERIFY(tool);
    QCOMPARE(tool->id, QString("call-1"));
    QCOMPARE(tool->name, QString("Edit main.cpp"));
    QCOMPARE(tool->kind, QString("edit"));
    QCOMPARE(tool->status, QString("completed"));
    QCOMPARE(tool->result, QString("done"));
    QVERIFY(tool->details.contains("locations"));

    QCOMPARE(session.rows().size(), 2);
    QCOMPARE(session.rows().at(1).kind, Session::RowKind::AgentTool);
    QCOMPARE(session.rows().at(1).toolStatus, QString("completed"));
    QCOMPARE(session.rows().at(1).toolKind, QString("edit"));
    QCOMPARE(session.rows().at(1).toolDetails, tool->details);
    QVERIFY(tool->fromAgent);
}

void SessionTest::testLlmToolLifecycleFlowsThroughOneEvent()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         llmToolStarted("r1", "t1", "read_file", QJsonObject{{"path", "main.cpp"}})});

    QCOMPARE(session.rows().size(), 2);
    QCOMPARE(session.rows().at(1).kind, Session::RowKind::Tool);
    QCOMPARE(session.rows().at(1).toolStatus, QString("in_progress"));

    backend.script(
        {Session::TextDelta{"r1", "working"},
         llmToolCompleted("r1", "t1", "read_file", "int main() {}"),
         Session::TextDelta{"r1", " still"}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);

    const auto *tool = std::get_if<Session::ToolCallBlock>(&assistant.blocks.at(0));
    QVERIFY(tool);
    QCOMPARE(tool->status, QString("completed"));
    QCOMPARE(tool->result, QString("int main() {}"));
    QCOMPARE(tool->arguments, (QJsonObject{{"path", "main.cpp"}}));
    QVERIFY(!tool->fromAgent);

    QCOMPARE(
        assistant.blocks.at(1), (Session::ContentBlock{Session::TextBlock{"working still"}}));
    QCOMPARE(session.rows().at(1).toolStatus, QString("completed"));
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void SessionTest::testSessionKeepsOnePlanPerTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"fix it"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::PlanUpdated{
             .turnId = "r1",
             .entries = {Session::PlanEntry{"Read", "high", "in_progress"},
                         Session::PlanEntry{"Fix", "medium", "pending"}}},
         Session::TextDelta{"r1", "working"},
         Session::PlanUpdated{
             .turnId = "r1",
             .entries = {Session::PlanEntry{"Read", "high", "completed"},
                         Session::PlanEntry{"Fix", "medium", "in_progress"}}}});

    const Session::Message &assistant = session.history().at(1);

    qsizetype planBlocks = 0;
    const Session::PlanBlock *plan = nullptr;
    for (const Session::ContentBlock &block : assistant.blocks) {
        if (const auto *candidate = std::get_if<Session::PlanBlock>(&block)) {
            ++planBlocks;
            plan = candidate;
        }
    }

    QCOMPARE(planBlocks, qsizetype(1));
    QVERIFY(plan);
    QCOMPARE(plan->entries.size(), 2);
    QCOMPARE(plan->entries.at(0).status, QString("completed"));
    QCOMPARE(plan->entries.at(1).status, QString("in_progress"));

    qsizetype planRows = 0;
    for (const Session::MessageRow &row : session.rows()) {
        if (row.kind == Session::RowKind::Plan)
            ++planRows;
    }
    QCOMPARE(planRows, qsizetype(1));
}

void SessionTest::testAgentActivityKeepsStreamedTextIntact()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Read main.cpp",
             .status = "pending",
             .fromAgent = true},
         Session::TextDelta{"r1", "Reading the file"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Read main.cpp",
             .status = "completed",
             .fromAgent = true},
         Session::TextDelta{"r1", " and it is done."}});

    const Session::Message &assistant = session.history().at(1);

    QStringList streamed;
    for (const Session::ContentBlock &block : assistant.blocks) {
        if (const auto *text = std::get_if<Session::TextBlock>(&block))
            streamed.append(text->text);
    }

    QCOMPARE(streamed, QStringList{"Reading the file and it is done."});
}

void SessionTest::testAgentToolUpdateTouchesOnlyItsOwnRow()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Read a",
             .status = "pending",
             .fromAgent = true},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-2",
             .name = "Read b",
             .status = "pending",
             .fromAgent = true}});

    QList<int> updatedRows;
    QObject::connect(
        &session,
        &Session::Session::rowUpdated,
        &session,
        [&updatedRows](int index, const Session::MessageRow &) { updatedRows.append(index); });

    backend.script(
        {Session::ToolCallUpdated{
            .turnId = "r1",
            .toolId = "call-1",
            .name = "Read a",
            .status = "completed",
            .fromAgent = true}});

    QCOMPARE(updatedRows, QList<int>{1});
    QCOMPARE(session.rows().at(1).toolStatus, QString("completed"));
    QCOMPARE(session.rows().at(2).toolStatus, QString("pending"));
}

void SessionTest::testUsageSkipsRowsThatCannotShowIt()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt);
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::PlanUpdated{
             .turnId = "r1", .entries = {Session::PlanEntry{"Read", "high", "in_progress"}}},
         Session::TextDelta{"r1", "here is the answer"},
         Session::UsageReported{"r1", Session::Usage{120, 40, 80, 12}}});

    const Session::MessageRow &planRow = session.rows().at(1);
    const Session::MessageRow &textRow = session.rows().at(2);

    QCOMPARE(planRow.kind, Session::RowKind::Plan);
    QVERIFY(planRow.usage.isEmpty());

    QCOMPARE(textRow.kind, Session::RowKind::Assistant);
    QCOMPARE(textRow.usage, (Session::Usage{120, 40, 80, 12}));
}

void SessionTest::testAgentActivityBlocksSurviveReload()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks
        = {Session::PlanBlock{
               {Session::PlanEntry{"Read", "high", "completed"},
                Session::PlanEntry{"Fix", "medium", "in_progress"}}},
           Session::ToolCallBlock{
               "call-1",
               "Edit main.cpp",
               QJsonObject{},
               "done",
               "edit",
               "completed",
               QJsonObject{{"diffs", QJsonArray{QJsonObject{{"path", "/p/main.cpp"}}}}},
               true}};

    Session::ConversationHistory history;
    history.append(assistant);

    const auto reloaded = Session::HistorySerializer::fromJson(
        Session::HistorySerializer::toJson(history));
    QVERIFY(reloaded.has_value());
    QCOMPARE(*reloaded, history);

    const QList<Session::MessageRow> rows = Session::projectToRows(history);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).kind, Session::RowKind::Plan);
    QCOMPARE(rows.at(1).kind, Session::RowKind::AgentTool);
    QVERIFY(Session::isTranscriptOnlyRow(rows.at(0).kind));
    QVERIFY(Session::isTranscriptOnlyRow(rows.at(1).kind));
    QCOMPARE(Session::buildFromRows(rows), history);
}

} // namespace QodeAssist
