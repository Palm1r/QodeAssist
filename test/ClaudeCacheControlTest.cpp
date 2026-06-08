// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "providers/ClaudeCacheControl.hpp"

using namespace QodeAssist::Providers::ClaudeCacheControl;

namespace {

QJsonObject expectedEphemeral(bool extendedTtl)
{
    QJsonObject obj{{"type", "ephemeral"}};
    if (extendedTtl)
        obj["ttl"] = "1h";
    return obj;
}

} // namespace

TEST(ClaudeCacheControlTest, BreakpointWithoutExtendedTTL)
{
    const QJsonObject cc = buildBreakpoint(false);
    EXPECT_EQ(cc.value("type").toString(), "ephemeral");
    EXPECT_FALSE(cc.contains("ttl"));
}

TEST(ClaudeCacheControlTest, BreakpointWithExtendedTTL)
{
    const QJsonObject cc = buildBreakpoint(true);
    EXPECT_EQ(cc.value("type").toString(), "ephemeral");
    EXPECT_EQ(cc.value("ttl").toString(), "1h");
}

TEST(ClaudeCacheControlTest, SystemAsStringWrappedIntoArray)
{
    QJsonObject request;
    request["system"] = "you are a helpful agent";

    apply(request, false);

    ASSERT_TRUE(request.value("system").isArray());
    const QJsonArray sys = request.value("system").toArray();
    ASSERT_EQ(sys.size(), 1);

    const QJsonObject block = sys.first().toObject();
    EXPECT_EQ(block.value("type").toString(), "text");
    EXPECT_EQ(block.value("text").toString(), "you are a helpful agent");
    EXPECT_EQ(block.value("cache_control").toObject(), expectedEphemeral(false));
}

TEST(ClaudeCacheControlTest, EmptySystemStringIsNotWrapped)
{
    QJsonObject request;
    request["system"] = "";

    apply(request, false);

    EXPECT_TRUE(request.value("system").isString());
}

TEST(ClaudeCacheControlTest, SystemAsArrayMarksLastBlock)
{
    QJsonObject request;
    request["system"] = QJsonArray{
        QJsonObject{{"type", "text"}, {"text", "a"}},
        QJsonObject{{"type", "text"}, {"text", "b"}}};

    apply(request, false);

    const QJsonArray sys = request.value("system").toArray();
    ASSERT_EQ(sys.size(), 2);
    EXPECT_FALSE(sys[0].toObject().contains("cache_control"));
    EXPECT_EQ(sys[1].toObject().value("cache_control").toObject(), expectedEphemeral(false));
}

TEST(ClaudeCacheControlTest, ToolsLastEntryGetsCacheControl)
{
    QJsonObject request;
    request["tools"] = QJsonArray{
        QJsonObject{{"name", "read_file"}},
        QJsonObject{{"name", "edit_file"}},
        QJsonObject{{"name", "search"}}};

    apply(request, true);

    const QJsonArray tools = request.value("tools").toArray();
    ASSERT_EQ(tools.size(), 3);
    EXPECT_FALSE(tools[0].toObject().contains("cache_control"));
    EXPECT_FALSE(tools[1].toObject().contains("cache_control"));
    EXPECT_EQ(tools[2].toObject().value("cache_control").toObject(), expectedEphemeral(true));
}

TEST(ClaudeCacheControlTest, SingleMessageHistorySkipped)
{
    QJsonObject request;
    request["messages"]
        = QJsonArray{QJsonObject{{"role", "user"}, {"content", "first message"}}};

    apply(request, false);

    const QJsonArray msgs = request.value("messages").toArray();
    ASSERT_EQ(msgs.size(), 1);
    EXPECT_TRUE(msgs[0].toObject().value("content").isString());
}

TEST(ClaudeCacheControlTest, HistoryBreakpointOnSecondToLastMessage)
{
    QJsonObject request;
    request["messages"] = QJsonArray{
        QJsonObject{{"role", "user"}, {"content", "u1"}},
        QJsonObject{{"role", "assistant"}, {"content", "a1"}},
        QJsonObject{{"role", "user"}, {"content", "u2-current"}}};

    apply(request, false);

    const QJsonArray msgs = request.value("messages").toArray();
    ASSERT_EQ(msgs.size(), 3);

    EXPECT_TRUE(msgs[0].toObject().value("content").isString());

    const QJsonArray a1Content = msgs[1].toObject().value("content").toArray();
    ASSERT_EQ(a1Content.size(), 1);
    EXPECT_EQ(a1Content.first().toObject().value("text").toString(), "a1");
    EXPECT_EQ(
        a1Content.first().toObject().value("cache_control").toObject(),
        expectedEphemeral(false));

    EXPECT_TRUE(msgs[2].toObject().value("content").isString());
}

TEST(ClaudeCacheControlTest, HistoryArrayContentMarksLastBlock)
{
    QJsonObject request;
    request["messages"] = QJsonArray{
        QJsonObject{
            {"role", "user"},
            {"content",
             QJsonArray{
                 QJsonObject{{"type", "text"}, {"text", "describe this"}},
                 QJsonObject{{"type", "image"}}}}},
        QJsonObject{{"role", "assistant"}, {"content", "ok"}}};

    apply(request, false);

    const QJsonArray msgs = request.value("messages").toArray();
    const QJsonArray content = msgs[0].toObject().value("content").toArray();
    ASSERT_EQ(content.size(), 2);
    EXPECT_FALSE(content[0].toObject().contains("cache_control"));
    EXPECT_EQ(content[1].toObject().value("cache_control").toObject(), expectedEphemeral(false));
}

TEST(ClaudeCacheControlTest, NoSystemNoToolsNoMessagesIsNoop)
{
    QJsonObject request;
    request["model"] = "claude-sonnet-4-5";
    request["max_tokens"] = 1024;

    apply(request, false);

    EXPECT_EQ(request.value("model").toString(), "claude-sonnet-4-5");
    EXPECT_EQ(request.value("max_tokens").toInt(), 1024);
    EXPECT_FALSE(request.contains("system"));
    EXPECT_FALSE(request.contains("tools"));
    EXPECT_FALSE(request.contains("messages"));
}

TEST(ClaudeCacheControlTest, EmptyToolsArrayIsNoop)
{
    QJsonObject request;
    request["tools"] = QJsonArray{};

    apply(request, false);

    EXPECT_TRUE(request.value("tools").isArray());
    EXPECT_TRUE(request.value("tools").toArray().isEmpty());
}
