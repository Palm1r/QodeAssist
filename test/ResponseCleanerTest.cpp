// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <ResponseCleaner.hpp>

using QodeAssist::ResponseCleaner;

TEST(ResponseCleanerTest, EmptyInputStaysEmpty)
{
    EXPECT_EQ(ResponseCleaner::clean(QString()), QString());
}

TEST(ResponseCleanerTest, PlainCodeIsUnchanged)
{
    const QString code = QStringLiteral("int main() {\n    return 0;\n}");
    EXPECT_EQ(ResponseCleaner::clean(code), code);
}

TEST(ResponseCleanerTest, ExtractsFencedCodeWithLanguage)
{
    const QString input = QStringLiteral("```cpp\nint main() {}\n```");
    EXPECT_EQ(ResponseCleaner::clean(input), QStringLiteral("int main() {}"));
}

TEST(ResponseCleanerTest, ExtractsFencedCodeWithoutLanguage)
{
    const QString input = QStringLiteral("```\nfoo\nbar\n```");
    EXPECT_EQ(ResponseCleaner::clean(input), QStringLiteral("foo\nbar"));
}

TEST(ResponseCleanerTest, StripsHeresTheExplanationPrefix)
{
    const QString input = QStringLiteral("Here's the refactored code:\nint x = 1;");
    EXPECT_EQ(ResponseCleaner::clean(input), QStringLiteral("int x = 1;"));
}

TEST(ResponseCleanerTest, StripsHereIsTheExplanationPrefix)
{
    const QString input = QStringLiteral("Here is the code:\nint y = 2;");
    EXPECT_EQ(ResponseCleaner::clean(input), QStringLiteral("int y = 2;"));
}

TEST(ResponseCleanerTest, StripsBareCodeColonPrefix)
{
    const QString input = QStringLiteral("code:\nfoo();");
    EXPECT_EQ(ResponseCleaner::clean(input), QStringLiteral("foo();"));
}

TEST(ResponseCleanerTest, TrimsLeadingAndTrailingNewlines)
{
    const QString input = QStringLiteral("\n\nhello\n\n");
    EXPECT_EQ(ResponseCleaner::clean(input), QStringLiteral("hello"));
}

TEST(ResponseCleanerTest, FencedCodeWithExplanationLineInsideIsExtractedVerbatim)
{
    // The fence body is returned verbatim; explanation stripping only inspects
    // the first lines of the *extracted* code, which here is real code.
    const QString input = QStringLiteral("```python\nx = 1\ny = 2\n```");
    EXPECT_EQ(ResponseCleaner::clean(input), QStringLiteral("x = 1\ny = 2"));
}
