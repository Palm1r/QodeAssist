// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ClaudeCacheControlTest.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "providers/ClaudeCacheControl.hpp"

namespace QodeAssist {

QJsonObject ClaudeCacheControlTest::expectedEphemeral(bool extendedTtl)
{
    QJsonObject cacheControl{{"type", "ephemeral"}};
    if (extendedTtl)
        cacheControl["ttl"] = "1h";
    return cacheControl;
}

void ClaudeCacheControlTest::testCacheControlBreakpointWithoutExtendedTtl()
{
    const QJsonObject cacheControl = Providers::ClaudeCacheControl::buildBreakpoint(false);

    QCOMPARE(cacheControl.value("type").toString(), QString("ephemeral"));
    QVERIFY(!cacheControl.contains("ttl"));
}

void ClaudeCacheControlTest::testCacheControlBreakpointWithExtendedTtl()
{
    const QJsonObject cacheControl = Providers::ClaudeCacheControl::buildBreakpoint(true);

    QCOMPARE(cacheControl.value("type").toString(), QString("ephemeral"));
    QCOMPARE(cacheControl.value("ttl").toString(), QString("1h"));
}

void ClaudeCacheControlTest::testCacheControlSystemAsStringWrappedIntoArray()
{
    QJsonObject request;
    request["system"] = "you are a helpful agent";

    Providers::ClaudeCacheControl::apply(request, false);

    QVERIFY(request.value("system").isArray());
    const QJsonArray system = request.value("system").toArray();
    QCOMPARE(system.size(), 1);

    const QJsonObject block = system.first().toObject();
    QCOMPARE(block.value("type").toString(), QString("text"));
    QCOMPARE(block.value("text").toString(), QString("you are a helpful agent"));
    QCOMPARE(block.value("cache_control").toObject(), expectedEphemeral(false));
}

void ClaudeCacheControlTest::testCacheControlEmptySystemStringIsNotWrapped()
{
    QJsonObject request;
    request["system"] = "";

    Providers::ClaudeCacheControl::apply(request, false);

    QVERIFY(request.value("system").isString());
}

void ClaudeCacheControlTest::testCacheControlSystemAsArrayMarksLastBlock()
{
    QJsonObject request;
    request["system"] = QJsonArray{
        QJsonObject{{"type", "text"}, {"text", "a"}}, QJsonObject{{"type", "text"}, {"text", "b"}}};

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray system = request.value("system").toArray();
    QCOMPARE(system.size(), 2);
    QVERIFY(!system[0].toObject().contains("cache_control"));
    QCOMPARE(system[1].toObject().value("cache_control").toObject(), expectedEphemeral(false));
}

void ClaudeCacheControlTest::testCacheControlToolsLastEntryGetsCacheControl()
{
    QJsonObject request;
    request["tools"] = QJsonArray{
        QJsonObject{{"name", "read_file"}},
        QJsonObject{{"name", "edit_file"}},
        QJsonObject{{"name", "search"}}};

    Providers::ClaudeCacheControl::apply(request, true);

    const QJsonArray tools = request.value("tools").toArray();
    QCOMPARE(tools.size(), 3);
    QVERIFY(!tools[0].toObject().contains("cache_control"));
    QVERIFY(!tools[1].toObject().contains("cache_control"));
    QCOMPARE(tools[2].toObject().value("cache_control").toObject(), expectedEphemeral(true));
}

void ClaudeCacheControlTest::testCacheControlSingleMessageHistorySkipped()
{
    QJsonObject request;
    request["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", "first message"}}};

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray messages = request.value("messages").toArray();
    QCOMPARE(messages.size(), 1);
    QVERIFY(messages[0].toObject().value("content").isString());
}

void ClaudeCacheControlTest::testCacheControlHistoryBreakpointOnSecondToLastMessage()
{
    QJsonObject request;
    request["messages"] = QJsonArray{
        QJsonObject{{"role", "user"}, {"content", "u1"}},
        QJsonObject{{"role", "assistant"}, {"content", "a1"}},
        QJsonObject{{"role", "user"}, {"content", "u2-current"}}};

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray messages = request.value("messages").toArray();
    QCOMPARE(messages.size(), 3);
    QVERIFY(messages[0].toObject().value("content").isString());

    const QJsonArray assistantContent = messages[1].toObject().value("content").toArray();
    QCOMPARE(assistantContent.size(), 1);
    QCOMPARE(assistantContent.first().toObject().value("text").toString(), QString("a1"));
    QCOMPARE(
        assistantContent.first().toObject().value("cache_control").toObject(),
        expectedEphemeral(false));

    QVERIFY(messages[2].toObject().value("content").isString());
}

void ClaudeCacheControlTest::testCacheControlHistoryArrayContentMarksLastBlock()
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

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray messages = request.value("messages").toArray();
    const QJsonArray content = messages[0].toObject().value("content").toArray();
    QCOMPARE(content.size(), 2);
    QVERIFY(!content[0].toObject().contains("cache_control"));
    QCOMPARE(content[1].toObject().value("cache_control").toObject(), expectedEphemeral(false));
}

void ClaudeCacheControlTest::testCacheControlNoSystemNoToolsNoMessagesIsNoop()
{
    QJsonObject request;
    request["model"] = "claude-sonnet-4-5";
    request["max_tokens"] = 1024;

    Providers::ClaudeCacheControl::apply(request, false);

    QCOMPARE(request.value("model").toString(), QString("claude-sonnet-4-5"));
    QCOMPARE(request.value("max_tokens").toInt(), 1024);
    QVERIFY(!request.contains("system"));
    QVERIFY(!request.contains("tools"));
    QVERIFY(!request.contains("messages"));
}

void ClaudeCacheControlTest::testCacheControlEmptyToolsArrayIsNoop()
{
    QJsonObject request;
    request["tools"] = QJsonArray{};

    Providers::ClaudeCacheControl::apply(request, false);

    QVERIFY(request.value("tools").isArray());
    QVERIFY(request.value("tools").toArray().isEmpty());
}

} // namespace QodeAssist
