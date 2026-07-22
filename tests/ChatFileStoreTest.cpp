// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatFileStoreTest.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "ChatView/ChatFileStore.hpp"
#include "acp/AgentBinding.hpp"
#include "session/HistorySerializer.hpp"
#include "session/Session.hpp"

namespace QodeAssist {

void ChatFileStoreTest::testChatFileStoreRoundTripsStoredContent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString chatFilePath = dir.filePath("chat.json");
    const QByteArray original = QByteArrayLiteral("int main() { return 0; }\n");

    QString storedPath;
    QVERIFY(Chat::ChatFileStore::saveContentToStorage(
        chatFilePath, "main.cpp", QString::fromLatin1(original.toBase64()), storedPath));
    QVERIFY(!storedPath.isEmpty());

    QCOMPARE(Chat::ChatFileStore::loadRawContentFromStorage(chatFilePath, storedPath), original);
    QCOMPARE(
        Chat::ChatFileStore::loadContentFromStorage(chatFilePath, storedPath),
        QString::fromLatin1(original.toBase64()));

    const QString contentFolder = Chat::ChatFileStore::getChatContentFolder(chatFilePath);
    QVERIFY(QFileInfo(QDir(contentFolder).filePath(storedPath)).exists());
}

void ChatFileStoreTest::testLegacyChatFileLoadsThroughTheFileStore()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonObject root;
    root["version"] = "0.2";
    root["messages"] = QJsonArray{
        QJsonObject{{"role", 1}, {"content", "explain this"}, {"id", "u1"}},
        QJsonObject{{"role", 2}, {"content", "here is the answer"}, {"id", "r1"}}};

    const QString filePath = dir.filePath("legacy.json");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    Session::ConversationHistory history;
    Acp::AgentBinding binding;
    const Chat::SerializationResult result
        = Chat::ChatFileStore::loadFromFile(history, binding, filePath);

    QVERIFY(result.success);
    QVERIFY(result.warningMessage.isEmpty());
    QVERIFY(binding.isEmpty());
    QCOMPARE(history.size(), 2);
    QCOMPARE(history.at(0).role, Session::MessageRole::User);
    QCOMPARE(
        std::get<Session::TextBlock>(history.at(0).blocks.at(0)),
        (Session::TextBlock{"explain this"}));
    QCOMPARE(history.at(1).role, Session::MessageRole::Assistant);
}

} // namespace QodeAssist
