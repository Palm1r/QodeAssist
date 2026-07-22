// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentBindingTest.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "ChatView/ChatFileStore.hpp"
#include "acp/AgentBinding.hpp"
#include "session/ConversationHistory.hpp"
#include "session/Session.hpp"

namespace QodeAssist {

void AgentBindingTest::testAgentBindingRejectsMalformedRecords()
{
    const auto rejected = [](const QJsonValue &value) {
        QString error;
        const Acp::AgentBinding binding = Acp::AgentBinding::fromJson(value, &error);
        return binding.isEmpty() && !error.isEmpty();
    };

    QVERIFY(rejected(QJsonValue(QStringLiteral("claude"))));
    QVERIFY(rejected(QJsonValue(42)));
    QVERIFY(rejected(QJsonObject{{"agentId", 7}}));
    QVERIFY(rejected(QJsonObject{{"agentId", "claude"}, {"sessionId", QJsonArray{}}}));
    QVERIFY(rejected(QJsonObject{{"sessionId", "orphan"}}));
    QVERIFY(rejected(QJsonObject{{"agentId", QString(4096, QChar('a'))}}));
    QVERIFY(rejected(QJsonObject{{"agentId", QStringLiteral("clau‮de")}}));

    QString error;
    QVERIFY(Acp::AgentBinding::fromJson(QJsonValue(), &error).isEmpty());
    QVERIFY(error.isEmpty());

    const Acp::AgentBinding good = Acp::AgentBinding::fromJson(
        QJsonObject{{"agentId", "claude"}, {"sessionId", "sess-1"}}, &error);
    QCOMPARE(good, (Acp::AgentBinding{"claude", "sess-1"}));
    QVERIFY(error.isEmpty());
}

void AgentBindingTest::testMalformedAgentBindingLeavesTheChatUnbound()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("broken_agent.json");

    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks = {Session::TextBlock{"hello"}};

    Session::ConversationHistory history;
    history.append(user);

    QVERIFY(Chat::ChatFileStore::saveToFile(history, Acp::AgentBinding{}, filePath).success);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    root["agent"] = QStringLiteral("claude");
    file.resize(0);
    file.write(QJsonDocument(root).toJson());
    file.close();

    Session::ConversationHistory reloaded;
    Acp::AgentBinding binding;
    const Chat::SerializationResult result
        = Chat::ChatFileStore::loadFromFile(reloaded, binding, filePath);

    QVERIFY(result.success);
    QVERIFY(binding.isEmpty());
    QVERIFY(!result.warningMessage.isEmpty());
}

void AgentBindingTest::testAgentBindingRoundTripsThroughTheChatFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("agent_chat.json");

    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks = {Session::TextBlock{"hello agent"}};

    Session::ConversationHistory history;
    history.append(user);

    const Acp::AgentBinding binding{"claude-code-acp", "sess-42"};
    QVERIFY(Chat::ChatFileStore::saveToFile(history, binding, filePath).success);

    Session::ConversationHistory reloaded;
    Acp::AgentBinding reloadedBinding;
    QVERIFY(Chat::ChatFileStore::loadFromFile(reloaded, reloadedBinding, filePath).success);

    QCOMPARE(reloaded, history);
    QCOMPARE(reloadedBinding, binding);
}

void AgentBindingTest::testChatFileWithoutAnAgentLoadsUnbound()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("llm_chat.json");

    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks = {Session::TextBlock{"hello"}};

    Session::ConversationHistory history;
    history.append(user);

    QVERIFY(Chat::ChatFileStore::saveToFile(history, Acp::AgentBinding{}, filePath).success);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(!root.contains("agent"));
    file.close();

    Session::ConversationHistory reloaded;
    Acp::AgentBinding reloadedBinding;
    QVERIFY(Chat::ChatFileStore::loadFromFile(reloaded, reloadedBinding, filePath).success);
    QVERIFY(reloadedBinding.isEmpty());
}

} // namespace QodeAssist
