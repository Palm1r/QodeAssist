// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <AgentConfig.hpp>
#include <AgentRouter.hpp>

using QodeAssist::AgentConfig;
using QodeAssist::AgentRouter::Context;
using QodeAssist::AgentRouter::matches;

namespace {

Context ctx(const QString &filePath, const QString &projectName = QString())
{
    return Context{filePath, projectName};
}

} // namespace

TEST(AgentRouterTest, EmptyMatchIsCatchAll)
{
    AgentConfig::Match m;
    EXPECT_TRUE(m.isEmpty());
    EXPECT_TRUE(matches(m, ctx(QStringLiteral("/any/file.cpp"))));
    EXPECT_TRUE(matches(m, ctx(QString())));
}

TEST(AgentRouterTest, FilePatternMatchesByFileName)
{
    AgentConfig::Match m;
    m.filePatterns = {QStringLiteral("*.cpp")};

    EXPECT_TRUE(matches(m, ctx(QStringLiteral("/proj/src/foo.cpp"))));
    EXPECT_TRUE(matches(m, ctx(QStringLiteral("foo.cpp"))));
    EXPECT_FALSE(matches(m, ctx(QStringLiteral("/proj/src/foo.h"))));
}

TEST(AgentRouterTest, FilePatternIsCaseInsensitive)
{
    AgentConfig::Match m;
    m.filePatterns = {QStringLiteral("*.cpp")};

    EXPECT_TRUE(matches(m, ctx(QStringLiteral("/proj/FOO.CPP"))));
}

TEST(AgentRouterTest, FilePatternWithEmptyPathDoesNotMatch)
{
    AgentConfig::Match m;
    m.filePatterns = {QStringLiteral("*.cpp")};

    EXPECT_FALSE(matches(m, ctx(QString())));
}

TEST(AgentRouterTest, MultipleFilePatternsAreOred)
{
    AgentConfig::Match m;
    m.filePatterns = {QStringLiteral("*.cpp"), QStringLiteral("*.qml")};

    EXPECT_TRUE(matches(m, ctx(QStringLiteral("main.qml"))));
    EXPECT_TRUE(matches(m, ctx(QStringLiteral("main.cpp"))));
    EXPECT_FALSE(matches(m, ctx(QStringLiteral("main.py"))));
}

TEST(AgentRouterTest, PathPatternMatchesAcrossSeparators)
{
    AgentConfig::Match m;
    m.pathPatterns = {QStringLiteral("*/tests/*")};

    EXPECT_TRUE(matches(m, ctx(QStringLiteral("/home/me/tests/x.cpp"))));
    EXPECT_FALSE(matches(m, ctx(QStringLiteral("/home/me/src/x.cpp"))));
}

TEST(AgentRouterTest, ProjectNameMatchIsCaseSensitive)
{
    AgentConfig::Match m;
    m.projectNames = {QStringLiteral("MyProj")};

    EXPECT_TRUE(matches(m, ctx(QStringLiteral("a.cpp"), QStringLiteral("MyProj"))));
    EXPECT_FALSE(matches(m, ctx(QStringLiteral("a.cpp"), QStringLiteral("myproj"))));
    EXPECT_FALSE(matches(m, ctx(QStringLiteral("a.cpp"), QString())));
}

TEST(AgentRouterTest, DimensionsAreAndedTogether)
{
    AgentConfig::Match m;
    m.filePatterns = {QStringLiteral("*.cpp")};
    m.projectNames = {QStringLiteral("P")};

    EXPECT_TRUE(matches(m, ctx(QStringLiteral("a.cpp"), QStringLiteral("P"))));
    EXPECT_FALSE(matches(m, ctx(QStringLiteral("a.cpp"), QStringLiteral("Q"))));
    EXPECT_FALSE(matches(m, ctx(QStringLiteral("a.h"), QStringLiteral("P"))));
}

TEST(AgentRouterTest, UnconstrainedDimensionDoesNotBlock)
{
    AgentConfig::Match m;
    m.projectNames = {QStringLiteral("P")};

    EXPECT_TRUE(matches(m, ctx(QString(), QStringLiteral("P"))));
}
