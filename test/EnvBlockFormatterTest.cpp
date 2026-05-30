// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <context/EnvBlockFormatter.hpp>

using namespace QodeAssist::Context;

TEST(EnvBlockFormatterTest, FormatProjectWithBuildDir)
{
    const QString out = EnvBlockFormatter::formatProject(
        {"MyApp", "/home/dev/myapp", "/home/dev/build-myapp"});

    EXPECT_TRUE(out.startsWith("# Active project: MyApp"));
    EXPECT_TRUE(out.contains("# Project source root: /home/dev/myapp"));
    EXPECT_TRUE(out.contains("# Build output directory"));
    EXPECT_TRUE(out.contains("/home/dev/build-myapp"));
}

TEST(EnvBlockFormatterTest, FormatProjectWithoutBuildDir)
{
    const QString out = EnvBlockFormatter::formatProject({"MyApp", "/home/dev/myapp", {}});

    EXPECT_TRUE(out.contains("# Project source root: /home/dev/myapp"));
    EXPECT_FALSE(out.contains("# Build output directory"));
}

TEST(EnvBlockFormatterTest, FormatProjectEmptyEnv)
{
    EXPECT_EQ(EnvBlockFormatter::formatProject({}), "# No active project in IDE");
}

TEST(EnvBlockFormatterTest, FormatFileWithKnownMime)
{
    const QString out
        = EnvBlockFormatter::formatFile({"/home/dev/myapp/main.cpp", "text/x-c++src"});

    EXPECT_TRUE(out.startsWith("File information:"));
    EXPECT_TRUE(out.contains("Language:"));
    EXPECT_TRUE(out.contains("text/x-c++src"));
    EXPECT_TRUE(out.contains("File path: /home/dev/myapp/main.cpp"));
}

TEST(EnvBlockFormatterTest, FormatFileWithoutMime)
{
    const QString out = EnvBlockFormatter::formatFile({"/home/dev/myapp/data.bin", {}});

    EXPECT_FALSE(out.contains("Language"));
    EXPECT_FALSE(out.contains("MIME"));
    EXPECT_TRUE(out.contains("File path: /home/dev/myapp/data.bin"));
}
