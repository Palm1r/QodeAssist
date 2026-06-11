// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <optional>

#include <QFuture>
#include <QHash>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QVector>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ToolResult.hpp>

#include <ConversationHistory.hpp>
#include <ErrorInfo.hpp>
#include <Message.hpp>
#include <ResponseEvent.hpp>
#include <ResponseRouter.hpp>

using namespace QodeAssist;

namespace {

class FakeClient : public LLMQore::BaseClient
{
public:
    using LLMQore::BaseClient::BaseClient;

    void fireChunk(const QString &id, const QString &chunk) { emit chunkReceived(id, chunk); }
    void fireThinking(const QString &id, const QString &thinking, const QString &signature)
    {
        emit thinkingBlockReceived(id, thinking, signature);
    }
    void fireToolStarted(
        const QString &id, const QString &toolId, const QString &name, const QJsonObject &args)
    {
        emit toolStarted(id, toolId, name, args);
    }
    void fireToolResult(
        const QString &id, const QString &toolId, const QString &name, const QString &result)
    {
        emit toolResultReady(id, toolId, name, result);
    }
    void fireFinalized(const QString &id, const LLMQore::CompletionInfo &info)
    {
        emit requestFinalized(id, info);
    }
    void fireFailed(const QString &id, const QString &error) { emit requestFailed(id, error); }

protected:
    LLMQore::RequestID sendMessage(
        const QJsonObject &, const QString &, LLMQore::RequestMode) override
    {
        return {};
    }
    LLMQore::RequestID ask(const QString &, LLMQore::RequestMode) override { return {}; }
    QFuture<QList<QString>> listModels() override { return {}; }
    LLMQore::ToolSchemaFormat toolSchemaFormat() const override
    {
        return LLMQore::ToolSchemaFormat::Claude;
    }
    void processData(const LLMQore::RequestID &, const QByteArray &) override {}
    void processBufferedResponse(const LLMQore::RequestID &, const QByteArray &) override {}
    QNetworkRequest prepareNetworkRequest(const QUrl &) const override { return {}; }
    LLMQore::BaseMessage *messageForRequest(const LLMQore::RequestID &) const override
    {
        return nullptr;
    }
    void cleanupDerivedData(const LLMQore::RequestID &) override {}
    QJsonObject buildContinuationPayload(
        const QJsonObject &,
        LLMQore::BaseMessage *,
        const QHash<QString, LLMQore::ToolResult> &) override
    {
        return {};
    }
};

} // namespace

TEST(ResponseRouterTest, BuildsAssistantTurnAndEmitsEvents)
{
    FakeClient client;
    ConversationHistory history;
    ResponseRouter router(&client, &history);

    QVector<ResponseEvent::Kind> kinds;
    QObject::connect(&router, &ResponseRouter::event, &router, [&kinds](const ResponseEvent &ev) {
        kinds.append(ev.kind());
    });

    const QString id = QStringLiteral("req-1");
    router.beginRequest(id);
    client.fireThinking(id, QStringLiteral("pondering"), QStringLiteral("sig"));
    client.fireChunk(id, QStringLiteral("Hello"));
    client.fireChunk(id, QStringLiteral(" world"));
    client.fireToolStarted(
        id, QStringLiteral("t1"), QStringLiteral("read_file"), QJsonObject{{"path", "a.txt"}});
    client.fireToolResult(
        id, QStringLiteral("t1"), QStringLiteral("read_file"), QStringLiteral("contents"));

    LLMQore::CompletionInfo info;
    info.stopReason = QStringLiteral("end_turn");
    info.usage = LLMQore::TokenUsage{12, 34, 0, 0};
    client.fireFinalized(id, info);

    ASSERT_EQ(history.size(), 2);

    const Message &assistant = history.messages()[0];
    EXPECT_EQ(assistant.role(), Message::Role::Assistant);
    EXPECT_EQ(assistant.id(), id);
    EXPECT_EQ(assistant.text(), QStringLiteral("Hello world"));
    EXPECT_TRUE(assistant.hasToolUse());

    const Message &toolResult = history.messages()[1];
    EXPECT_EQ(toolResult.role(), Message::Role::User);

    EXPECT_TRUE(kinds.contains(ResponseEvent::Kind::ThinkingDelta));
    EXPECT_TRUE(kinds.contains(ResponseEvent::Kind::TextDelta));
    EXPECT_TRUE(kinds.contains(ResponseEvent::Kind::ToolResult));
    EXPECT_TRUE(kinds.contains(ResponseEvent::Kind::Usage));
    EXPECT_TRUE(kinds.contains(ResponseEvent::Kind::MessageStop));
}

TEST(ResponseRouterTest, CategorizesAuthError)
{
    FakeClient client;
    ConversationHistory history;
    ResponseRouter router(&client, &history);

    std::optional<ResponseEvents::Error> captured;
    QObject::connect(&router, &ResponseRouter::event, &router, [&captured](const ResponseEvent &ev) {
        if (ev.kind() == ResponseEvent::Kind::Error)
            captured = *ev.as<ResponseEvents::Error>();
    });

    router.beginRequest(QStringLiteral("req-2"));
    client.fireFailed(
        QStringLiteral("req-2"), QStringLiteral("HTTP 401 Unauthorized: invalid api key"));

    ASSERT_TRUE(captured.has_value());
    EXPECT_EQ(captured->category, ErrorCategory::Auth);
}

TEST(ResponseRouterTest, IgnoresEventsForInactiveRequest)
{
    FakeClient client;
    ConversationHistory history;
    ResponseRouter router(&client, &history);

    router.beginRequest(QStringLiteral("req-3"));
    client.fireChunk(QStringLiteral("OTHER"), QStringLiteral("ignored"));

    EXPECT_TRUE(history.isEmpty());
}
