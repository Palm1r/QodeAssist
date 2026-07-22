// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatViewTest.hpp"

#include <QTest>

#include "ChatView/ChatModel.hpp"
#include "ChatView/FileMentionItem.hpp"
#include "session/HistoryProjection.hpp"
#include "session/Session.hpp"
#include "settings/ChatAssistantSettings.hpp"

namespace QodeAssist {

void ChatViewTest::testFileMentionSelectionFollowsChatToolsSetting()
{
    auto &enableChatTools = Settings::chatAssistantSettings().enableChatTools;
    const bool originalValue = enableChatTools();
    const auto restore = qScopeGuard(
        [&] { enableChatTools.setValue(originalValue, Utils::BaseAspect::BeQuiet); });

    Chat::FileMentionItem item;
    QStringList attachRequests;
    connect(
        &item,
        &Chat::FileMentionItem::fileAttachRequested,
        this,
        [&attachRequests](const QStringList &paths) { attachRequests += paths; });

    enableChatTools.setValue(true, Utils::BaseAspect::BeQuiet);
    QVariantMap result = item.handleFileSelection("/proj/src/main.cpp", "src/main.cpp", "proj", "main");
    QCOMPARE(result.value("mode").toString(), QString("mention"));
    QCOMPARE(result.value("mentionText").toString(), QString("@main.cpp "));
    QVERIFY(attachRequests.isEmpty());

    enableChatTools.setValue(false, Utils::BaseAspect::BeQuiet);
    result = item.handleFileSelection("/proj/src/main.cpp", "src/main.cpp", "proj", "main");
    QCOMPARE(result.value("mode").toString(), QString("attach"));
    QCOMPARE(attachRequests, QStringList{"/proj/src/main.cpp"});
}

void ChatViewTest::testChatModelExposesSessionRowsDirectly()
{
    Chat::ChatModel model;

    Session::MessageRow tool;
    tool.kind = Session::RowKind::AgentTool;
    tool.id = "t1";
    tool.toolKind = "edit";
    tool.toolStatus = "completed";
    tool.toolDetails = QJsonObject{{"diffs", QJsonArray{QJsonObject{{"path", "/p/main.cpp"}}}}};
    tool.usage = Session::Usage{10, 5, 0, 0};

    model.resetMessages({tool});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(
        model.data(model.index(0), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::Tool);
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::ToolKind).toString(), QString("edit"));

    const QVariant details = model.data(model.index(0), Chat::ChatModel::ToolDetails);
    QCOMPARE(details.value<QJsonObject>(), tool.toolDetails);
    QVERIFY(details.toMap().contains("diffs"));

    QCOMPARE(model.data(model.index(0), Chat::ChatModel::PromptTokens).toInt(), 10);
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::TotalTokens).toInt(), 15);

    Session::MessageRow updated = tool;
    updated.toolStatus = "failed";
    model.updateMessage(0, updated);
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::ToolStatus).toString(), QString("failed"));
}

} // namespace QodeAssist
