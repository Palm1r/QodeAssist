// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatHistorySerializerTest.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "ChatView/ChatModel.hpp"
#include "SessionTestSupport.hpp"
#include "session/HistoryProjection.hpp"
#include "session/HistorySerializer.hpp"
#include "session/Session.hpp"

namespace QodeAssist {

void ChatHistorySerializerTest::testHistorySnapshotUsesCurrentVersion()
{
    const QJsonObject root = Session::HistorySerializer::toJson(sampleHistory());

    QCOMPARE(root.value("version").toString(), Session::HistorySerializer::currentVersion());
    QCOMPARE(root.value("messages").toArray().size(), 2);
}

void ChatHistorySerializerTest::testHistorySnapshotRoundTrip()
{
    const Session::ConversationHistory history = sampleHistory();

    const auto restored = Session::HistorySerializer::fromJson(
        Session::HistorySerializer::toJson(history));

    QVERIFY(restored.has_value());
    QCOMPARE(restored->size(), history.size());
    QCOMPARE(restored->at(1).blocks.size(), history.at(1).blocks.size());
    QCOMPARE(*restored, history);
}

void ChatHistorySerializerTest::testUnsupportedChatVersionIsRejected()
{
    QJsonObject root;
    root["version"] = "9.9";
    root["messages"] = QJsonArray{};

    QVERIFY(!Session::HistorySerializer::fromJson(root).has_value());
    QVERIFY(!Session::HistorySerializer::isSupportedVersion("9.9"));
    QVERIFY(Session::HistorySerializer::isSupportedVersion("0.1"));
    QVERIFY(Session::HistorySerializer::isSupportedVersion("0.2"));
}

void ChatHistorySerializerTest::testChatFileWithoutMessagesArrayIsRejected()
{
    QJsonObject missing;
    missing["version"] = Session::HistorySerializer::currentVersion();
    QVERIFY(!Session::HistorySerializer::fromJson(missing).has_value());

    QJsonObject wrongType;
    wrongType["version"] = Session::HistorySerializer::currentVersion();
    wrongType["messages"] = "not an array";
    QVERIFY(!Session::HistorySerializer::fromJson(wrongType).has_value());

    QJsonObject empty;
    empty["version"] = Session::HistorySerializer::currentVersion();
    empty["messages"] = QJsonArray{};
    const auto history = Session::HistorySerializer::fromJson(empty);
    QVERIFY(history.has_value());
    QCOMPARE(history->size(), 0);
}

void ChatHistorySerializerTest::testLegacyChatFileConvertsToHistory()
{
    QJsonObject root;
    root["version"] = "0.2";
    root["messages"] = QJsonArray{
        QJsonObject{{"role", 1}, {"content", "explain this"}, {"id", "u1"}},
        QJsonObject{
            {"role", 5},
            {"content", "weighing options\n[Signature: sig-1...]"},
            {"id", "r1"},
            {"signature", "sig-1"},
            {"usage", QJsonObject{{"promptTokens", 120}, {"completionTokens", 40}}}},
        QJsonObject{
            {"role", 3},
            {"content", "read_file\nint main() {}"},
            {"id", "t1"},
            {"toolName", "read_file"},
            {"toolResult", "int main() {}"}},
        QJsonObject{{"role", 2}, {"content", "here is the answer"}, {"id", "r1"}}};

    const auto history = Session::HistorySerializer::fromJson(root);

    QVERIFY(history.has_value());
    QCOMPARE(history->size(), 2);

    const Session::Message &user = history->at(0);
    QCOMPARE(user.role, Session::MessageRole::User);
    QCOMPARE(user.blocks.size(), 1);
    QCOMPARE(std::get<Session::TextBlock>(user.blocks.at(0)), (Session::TextBlock{"explain this"}));

    const Session::Message &assistant = history->at(1);
    QCOMPARE(assistant.role, Session::MessageRole::Assistant);
    QCOMPARE(assistant.id, QString("r1"));
    QCOMPARE(assistant.blocks.size(), 3);
    QCOMPARE(
        std::get<Session::ThinkingBlock>(assistant.blocks.at(0)),
        (Session::ThinkingBlock{"weighing options", "sig-1", false}));
    QCOMPARE(
        std::get<Session::ToolCallBlock>(assistant.blocks.at(1)),
        (Session::ToolCallBlock{"t1", "read_file", QJsonObject{}, "int main() {}"}));
    QCOMPARE(
        std::get<Session::TextBlock>(assistant.blocks.at(2)),
        (Session::TextBlock{"here is the answer"}));
    QCOMPARE(assistant.usage.promptTokens, 120);
    QCOMPARE(assistant.usage.completionTokens, 40);
}

void ChatHistorySerializerTest::testLegacyToolRowWithoutMetadataKeepsItsText()
{
    QJsonObject root;
    root["version"] = "0.2";
    root["messages"] = QJsonArray{
        QJsonObject{{"role", 1}, {"content", "list the files"}, {"id", "u1"}},
        QJsonObject{{"role", 3}, {"content", "main.cpp\nmain.qml"}, {"id", "t1"}}};

    const auto history = Session::HistorySerializer::fromJson(root);
    QVERIFY(history.has_value());

    const QList<Session::MessageRow> rows = Session::projectToRows(*history);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(1).kind, Session::RowKind::Tool);
    QCOMPARE(rows.at(1).id, QString("t1"));
    QCOMPARE(rows.at(1).content, QString("main.cpp\nmain.qml"));
}

void ChatHistorySerializerTest::testChatRowsRoundTripThroughHistory()
{
    Session::MessageRow user;
    user.kind = Session::RowKind::User;
    user.id = "u1";
    user.content = "explain this";
    user.attachments = {Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}};
    user.images = {Session::ImageBlock{"shot.png", "shot_cd34.png", "image/png"}};

    Session::MessageRow thinking;
    thinking.kind = Session::RowKind::Thinking;
    thinking.id = "r1";
    thinking.content = "weighing options\n[Signature: sig-1...]";
    thinking.signature = "sig-1";
    thinking.usage = Session::Usage{120, 40, 80, 12};

    Session::MessageRow tool;
    tool.kind = Session::RowKind::Tool;
    tool.id = "t1";
    tool.content = "read_file\nint main() {}";
    tool.toolName = "read_file";
    tool.toolArguments = QJsonObject{{"path", "main.cpp"}};
    tool.toolResult = "int main() {}";

    Session::MessageRow assistant;
    assistant.kind = Session::RowKind::Assistant;
    assistant.id = "r1";
    assistant.content = "here is the answer";

    Session::MessageRow fileEdit;
    fileEdit.kind = Session::RowKind::FileEdit;
    fileEdit.id = "e1";
    fileEdit.content = "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\"}";

    const QList<Session::MessageRow> rows{user, thinking, tool, assistant, fileEdit};

    const Session::ConversationHistory history = Session::buildFromRows(rows);
    QCOMPARE(history.size(), 2);

    const QList<Session::MessageRow> projected = Session::projectToRows(history);
    QCOMPARE(projected, rows);
}

void ChatHistorySerializerTest::testCompressedChatShapeReloadsAsOneAssistantRow()
{
    Session::Message summary;
    summary.role = Session::MessageRole::Assistant;
    summary.id = "c1";
    summary.blocks = {Session::TextBlock{"# Chat Summary\n\nwe discussed the parser"}};

    Session::ConversationHistory compressed;
    compressed.append(summary);

    const auto reloaded = Session::HistorySerializer::fromJson(
        Session::HistorySerializer::toJson(compressed));
    QVERIFY(reloaded.has_value());

    const QList<Session::MessageRow> rows = Session::projectToRows(*reloaded);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).kind, Session::RowKind::Assistant);
    QCOMPARE(rows.at(0).content, QString("# Chat Summary\n\nwe discussed the parser"));
}

void ChatHistorySerializerTest::testHistoryAppliesToChatModel()
{
    Chat::ChatModel model;
    Session::Session session;
    attachModelToSession(session, model);

    session.setHistory(historyWithoutFileEdits());

    QCOMPARE(model.rowCount(), 4);

    QCOMPARE(
        model.data(model.index(0), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::User);
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::Content).toString(), QString("explain this"));
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::Attachments).toList().size(), 1);
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::Images).toList().size(), 1);

    QCOMPARE(
        model.data(model.index(1), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::Thinking);
    QCOMPARE(
        model.data(model.index(1), Chat::ChatModel::Content).toString(),
        QString("weighing options\n[Signature: sig-1...]"));

    QCOMPARE(
        model.data(model.index(2), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::Tool);
    QCOMPARE(
        model.data(model.index(2), Chat::ChatModel::Content).toString(),
        QString("read_file\nint main() {}"));

    QCOMPARE(
        model.data(model.index(3), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::Assistant);
    QCOMPARE(
        model.data(model.index(3), Chat::ChatModel::Content).toString(),
        QString("here is the answer"));

    QCOMPARE(model.sessionPromptTokens(), 120);
    QCOMPARE(model.sessionCompletionTokens(), 40);
    QCOMPARE(model.sessionCachedPromptTokens(), 80);
}

void ChatHistorySerializerTest::testAdjacentThinkingBlocksSurviveReload()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks
        = {Session::ThinkingBlock{"first", "sig-a", true},
           Session::ThinkingBlock{"second", "sig-b", true},
           Session::TextBlock{"the answer"}};

    Session::ConversationHistory history;
    history.append(assistant);

    const QList<Session::MessageRow> rows = Session::projectToRows(history);

    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).content, QString("first\n[Signature: sig-a...]"));
    QCOMPARE(rows.at(1).content, QString("second\n[Signature: sig-b...]"));
    QCOMPARE(Session::buildFromRows(rows), history);
}

} // namespace QodeAssist
