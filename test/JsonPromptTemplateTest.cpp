// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include <AgentConfig.hpp>
#include <ContextData.hpp>
#include <JsonPromptTemplate.hpp>

using QodeAssist::AgentConfig;
using QodeAssist::Templates::ContextData;
using QodeAssist::Templates::JsonPromptTemplate;

namespace {

AgentConfig makeConfig(const QJsonObject &body)
{
    AgentConfig cfg;
    cfg.name = QStringLiteral("test-agent");
    cfg.body = body;
    return cfg;
}

const QString kUserMessages = QStringLiteral(
    "[ { \"role\": \"user\", \"content\": {{ tojson(ctx.prefix) }} } ]");

const QString kSystemField = QStringLiteral(
    "{% if existsIn(ctx, \"system_prompt\") %}{{ tojson(ctx.system_prompt) }}{% endif %}");

} // namespace

TEST(JsonPromptTemplateTest, RendersJinjaSplicesAndKeepsLiterals)
{
    auto tmpl = JsonPromptTemplate::fromConfig(makeConfig(
        QJsonObject{
            {"max_tokens", 128},
            {"temperature", 0.5},
            {"stream", true},
            {"messages", kUserMessages},
        }));
    ASSERT_NE(tmpl, nullptr);

    ContextData ctx;
    ctx.prefix = QStringLiteral("hello world");

    QJsonObject request;
    ASSERT_TRUE(tmpl->buildFullRequest(request, ctx));

    EXPECT_EQ(request.value("max_tokens").toInt(), 128);
    EXPECT_DOUBLE_EQ(request.value("temperature").toDouble(), 0.5);
    EXPECT_TRUE(request.value("stream").toBool());

    const QJsonArray messages = request.value("messages").toArray();
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages.at(0).toObject().value("content").toString(), QStringLiteral("hello world"));
}

TEST(JsonPromptTemplateTest, DropsKeyWhenJinjaRendersEmpty)
{
    auto tmpl = JsonPromptTemplate::fromConfig(makeConfig(
        QJsonObject{
            {"system", kSystemField},
            {"messages", kUserMessages},
        }));
    ASSERT_NE(tmpl, nullptr);

    ContextData ctx;
    ctx.prefix = QStringLiteral("hi");

    QJsonObject request;
    ASSERT_TRUE(tmpl->buildFullRequest(request, ctx));

    EXPECT_FALSE(request.contains(QStringLiteral("system")));
    EXPECT_TRUE(request.contains(QStringLiteral("messages")));
}

TEST(JsonPromptTemplateTest, RendersSystemPromptWhenPresent)
{
    auto tmpl = JsonPromptTemplate::fromConfig(makeConfig(
        QJsonObject{
            {"system", kSystemField},
            {"messages", kUserMessages},
        }));
    ASSERT_NE(tmpl, nullptr);

    ContextData ctx;
    ctx.prefix = QStringLiteral("hi");
    ctx.systemPrompt = QStringLiteral("You are a helpful assistant.");

    QJsonObject request;
    ASSERT_TRUE(tmpl->buildFullRequest(request, ctx));

    EXPECT_EQ(request.value("system").toString(), QStringLiteral("You are a helpful assistant."));
}

TEST(JsonPromptTemplateTest, PreservesNestedLiteralObjects)
{
    auto tmpl = JsonPromptTemplate::fromConfig(makeConfig(
        QJsonObject{
            {"thinking", QJsonObject{{"type", "adaptive"}, {"budget", 8192}}},
            {"messages", kUserMessages},
        }));
    ASSERT_NE(tmpl, nullptr);

    ContextData ctx;
    ctx.prefix = QStringLiteral("x");

    QJsonObject request;
    ASSERT_TRUE(tmpl->buildFullRequest(request, ctx));

    const QJsonObject thinking = request.value("thinking").toObject();
    EXPECT_EQ(thinking.value("type").toString(), QStringLiteral("adaptive"));
    EXPECT_EQ(thinking.value("budget").toInt(), 8192);
}

TEST(JsonPromptTemplateTest, RejectsBodyThatRendersInvalidJsonAtLoad)
{
    QString error;
    auto tmpl = JsonPromptTemplate::fromConfig(
        makeConfig(
            QJsonObject{
                {"messages", QStringLiteral("[ {{ tojson(ctx.prefix) }}")},
            }),
        &error);

    EXPECT_EQ(tmpl, nullptr);
    EXPECT_FALSE(error.isEmpty());
}

TEST(JsonPromptTemplateTest, RejectsHandQuotedInterpolationAtLoad)
{
    QString error;
    auto tmpl = JsonPromptTemplate::fromConfig(
        makeConfig(
            QJsonObject{
                {"messages",
                 QStringLiteral("[ { \"role\": \"user\", \"content\": \"{{ ctx.prefix }}\" } ]")},
            }),
        &error);

    EXPECT_EQ(tmpl, nullptr);
    EXPECT_FALSE(error.isEmpty());
}

TEST(JsonPromptTemplateTest, RoundTripsQuotesBackslashesAndNewlinesViaTojson)
{
    auto tmpl = JsonPromptTemplate::fromConfig(makeConfig(
        QJsonObject{
            {"messages", kUserMessages},
        }));
    ASSERT_NE(tmpl, nullptr);

    ContextData ctx;
    ctx.prefix = QStringLiteral("a \"quoted\" back\\slash\nnewline");

    QJsonObject request;
    ASSERT_TRUE(tmpl->buildFullRequest(request, ctx));
    const QJsonArray messages = request.value("messages").toArray();
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages.at(0).toObject().value("content").toString(), *ctx.prefix);
}
