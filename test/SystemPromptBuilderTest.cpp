// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QSignalSpy>

#include <SystemPromptBuilder.hpp>

using QodeAssist::SystemPromptBuilder;

TEST(SystemPromptBuilderTest, StartsEmpty)
{
    SystemPromptBuilder builder;
    EXPECT_TRUE(builder.isEmpty());
    EXPECT_TRUE(builder.compose().isEmpty());
    EXPECT_TRUE(builder.layerNames().isEmpty());
}

TEST(SystemPromptBuilderTest, SetLayerStoresTextAndName)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("agent"), QStringLiteral("you are helpful"));

    EXPECT_FALSE(builder.isEmpty());
    EXPECT_EQ(builder.layer(QStringLiteral("agent")), QStringLiteral("you are helpful"));
    EXPECT_EQ(builder.layerNames(), QStringList{QStringLiteral("agent")});
}

TEST(SystemPromptBuilderTest, ComposeOrdersByPriorityAscending)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("b"), QStringLiteral("B"), 100);
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"), 50);

    EXPECT_EQ(builder.compose(), QStringLiteral("A\n\nB"));
}

TEST(SystemPromptBuilderTest, EqualPriorityKeepsInsertionOrder)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("first"), QStringLiteral("F"), 100);
    builder.setLayer(QStringLiteral("second"), QStringLiteral("S"), 100);

    EXPECT_EQ(builder.compose(), QStringLiteral("F\n\nS"));
}

TEST(SystemPromptBuilderTest, AgentPriorityComposesBeforeDefault)
{
    SystemPromptBuilder builder;
    builder.setLayer(
        QStringLiteral("env"), QStringLiteral("ENV"), SystemPromptBuilder::kDefaultPriority);
    builder.setLayer(
        QStringLiteral("agent"), QStringLiteral("SYS"), SystemPromptBuilder::kAgentPriority);

    EXPECT_EQ(builder.compose(), QStringLiteral("SYS\n\nENV"));
}

TEST(SystemPromptBuilderTest, ComposeUsesCustomSeparator)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"), 1);
    builder.setLayer(QStringLiteral("b"), QStringLiteral("B"), 2);

    EXPECT_EQ(builder.compose(QStringLiteral(" | ")), QStringLiteral("A | B"));
}

TEST(SystemPromptBuilderTest, ComposeSkipsEmptyTextButLayerStaysNamed)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"));
    builder.setLayer(QStringLiteral("blank"), QString());

    EXPECT_EQ(builder.compose(), QStringLiteral("A"));
    EXPECT_TRUE(builder.layerNames().contains(QStringLiteral("blank")));
}

TEST(SystemPromptBuilderTest, SetLayerUpdatesExistingInPlace)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("old"));
    builder.setLayer(QStringLiteral("a"), QStringLiteral("new"));

    EXPECT_EQ(builder.layerNames().size(), 1);
    EXPECT_EQ(builder.layer(QStringLiteral("a")), QStringLiteral("new"));
}

TEST(SystemPromptBuilderTest, IdenticalSetLayerEmitsNoSignal)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"), 10);

    QSignalSpy spy(&builder, &SystemPromptBuilder::layersChanged);
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"), 10);

    EXPECT_EQ(spy.size(), 0);
}

TEST(SystemPromptBuilderTest, ChangingSetLayerEmitsSignal)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"), 10);

    QSignalSpy spy(&builder, &SystemPromptBuilder::layersChanged);
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"), 20);

    EXPECT_EQ(spy.size(), 1);
}

TEST(SystemPromptBuilderTest, ClearLayerRemovesAndSignals)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"));

    QSignalSpy spy(&builder, &SystemPromptBuilder::layersChanged);
    builder.clearLayer(QStringLiteral("a"));

    EXPECT_TRUE(builder.isEmpty());
    EXPECT_EQ(spy.size(), 1);
}

TEST(SystemPromptBuilderTest, ClearMissingLayerEmitsNoSignal)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"));

    QSignalSpy spy(&builder, &SystemPromptBuilder::layersChanged);
    builder.clearLayer(QStringLiteral("nope"));

    EXPECT_FALSE(builder.isEmpty());
    EXPECT_EQ(spy.size(), 0);
}

TEST(SystemPromptBuilderTest, ClearEmptiesAndSignals)
{
    SystemPromptBuilder builder;
    builder.setLayer(QStringLiteral("a"), QStringLiteral("A"));
    builder.setLayer(QStringLiteral("b"), QStringLiteral("B"));

    QSignalSpy spy(&builder, &SystemPromptBuilder::layersChanged);
    builder.clear();

    EXPECT_TRUE(builder.isEmpty());
    EXPECT_EQ(spy.size(), 1);
}

TEST(SystemPromptBuilderTest, ClearWhenAlreadyEmptyEmitsNoSignal)
{
    SystemPromptBuilder builder;

    QSignalSpy spy(&builder, &SystemPromptBuilder::layersChanged);
    builder.clear();

    EXPECT_EQ(spy.size(), 0);
}
