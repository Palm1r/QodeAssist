// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <ContextRenderer.hpp>

using QodeAssist::Templates::ContextRenderer::Bindings;
using QodeAssist::Templates::ContextRenderer::render;

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(contents);
}

} // namespace

TEST(ContextRendererTest, EmptyTemplateRendersEmpty)
{
    EXPECT_TRUE(render(QString(), Bindings{}).isEmpty());
}

TEST(ContextRendererTest, SubstitutesProjectDirAndConfigVariables)
{
    const QString out = render(
        QStringLiteral("P=${PROJECT_DIR};C=${CONFIG_DIR}"),
        Bindings{QStringLiteral("/proj"), QStringLiteral("/cfg")});
    EXPECT_EQ(out, QStringLiteral("P=/proj;C=/cfg"));
}

TEST(ContextRendererTest, ReadsFileWithinProjectDir)
{
    QTemporaryDir proj;
    ASSERT_TRUE(proj.isValid());
    writeFile(proj.filePath(QStringLiteral("notes.txt")), "hello body");

    const QString out = render(
        QStringLiteral("{{ read_file(\"${PROJECT_DIR}/notes.txt\") }}"),
        Bindings{proj.path(), QString()});
    EXPECT_EQ(out, QStringLiteral("hello body"));
}

TEST(ContextRendererTest, ReadsFileUnderConfigDir)
{
    QTemporaryDir config;
    ASSERT_TRUE(config.isValid());
    writeFile(config.filePath(QStringLiteral("persona.md")), "be terse");

    const QString out = render(
        QStringLiteral("{{ read_file(\"${CONFIG_DIR}/persona.md\") }}"),
        Bindings{QStringLiteral("/unrelated/project"), config.path()});
    EXPECT_EQ(out, QStringLiteral("be terse"));
}

TEST(ContextRendererTest, ReadFileOutsideAllowedRootsThrowsLoudly)
{
    QTemporaryDir proj;
    QTemporaryDir outside;
    ASSERT_TRUE(proj.isValid());
    ASSERT_TRUE(outside.isValid());
    writeFile(outside.filePath(QStringLiteral("secret.txt")), "TOP SECRET");

    QString error;
    const QString out = render(
        QStringLiteral("{{ read_file(\"%1/secret.txt\") }}").arg(outside.path()),
        Bindings{proj.path(), QString()},
        &error);

    EXPECT_TRUE(out.isEmpty());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(error.contains(QStringLiteral("read_file")));
    EXPECT_TRUE(error.contains(QStringLiteral("outside the allowed read roots")));
}

TEST(ContextRendererTest, ReadFileMissingButAllowedThrowsLoudly)
{
    QTemporaryDir proj;
    ASSERT_TRUE(proj.isValid());

    QString error;
    const QString out = render(
        QStringLiteral("{{ read_file(\"${PROJECT_DIR}/nope.txt\") }}"),
        Bindings{proj.path(), QString()},
        &error);

    EXPECT_TRUE(out.isEmpty());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(error.contains(QStringLiteral("cannot open")));
}

TEST(ContextRendererTest, FileExistsTrueForPresentAllowedFileAndFalseWhenAbsent)
{
    QTemporaryDir proj;
    ASSERT_TRUE(proj.isValid());
    writeFile(proj.filePath(QStringLiteral("present.txt")), "x");

    EXPECT_EQ(
        render(
            QStringLiteral("{{ file_exists(\"${PROJECT_DIR}/present.txt\") }}"),
            Bindings{proj.path(), QString()}),
        QStringLiteral("true"));

    QString error;
    EXPECT_EQ(
        render(
            QStringLiteral("{{ file_exists(\"${PROJECT_DIR}/missing.txt\") }}"),
            Bindings{proj.path(), QString()},
            &error),
        QStringLiteral("false"));
    EXPECT_TRUE(error.isEmpty());
}

TEST(ContextRendererTest, FileExistsOutsideAllowedRootsThrowsLoudly)
{
    QTemporaryDir proj;
    QTemporaryDir outside;
    ASSERT_TRUE(proj.isValid());
    ASSERT_TRUE(outside.isValid());
    writeFile(outside.filePath(QStringLiteral("present.txt")), "x");

    QString error;
    const QString out = render(
        QStringLiteral("{{ file_exists(\"%1/present.txt\") }}").arg(outside.path()),
        Bindings{proj.path(), QString()},
        &error);

    EXPECT_TRUE(out.isEmpty());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(error.contains(QStringLiteral("file_exists")));
}

TEST(ContextRendererTest, FileExistsWithUnresolvedProjectDirReturnsFalse)
{
    QString error;
    EXPECT_EQ(
        render(
            QStringLiteral(
                "{% if file_exists(\"${PROJECT_DIR}/x.md\") %}yes{% else %}no{% endif %}"),
            Bindings{QString(), QString()},
            &error),
        QStringLiteral("no"));
    EXPECT_TRUE(error.isEmpty());
}

TEST(ContextRendererTest, ReadFileWithUnresolvedProjectDirThrowsLoudly)
{
    QString error;
    const QString out = render(
        QStringLiteral("{{ read_file(\"${PROJECT_DIR}/x.md\") }}"),
        Bindings{QString(), QString()},
        &error);
    EXPECT_TRUE(out.isEmpty());
    EXPECT_TRUE(error.contains(QStringLiteral("read_file")));
}

TEST(ContextRendererTest, HeadLinesTakesLeadingLines)
{
    QTemporaryDir proj;
    ASSERT_TRUE(proj.isValid());
    writeFile(proj.filePath(QStringLiteral("multi.txt")), "l1\nl2\nl3\n");

    const QString out = render(
        QStringLiteral("{{ head_lines(read_file(\"${PROJECT_DIR}/multi.txt\"), 2) }}"),
        Bindings{proj.path(), QString()});
    EXPECT_EQ(out, QStringLiteral("l1\nl2"));
}

TEST(ContextRendererTest, StringHelpers)
{
    const Bindings none{};
    EXPECT_EQ(render(QStringLiteral("{{ basename(\"/a/b/c.txt\") }}"), none), QStringLiteral("c.txt"));
    EXPECT_EQ(render(QStringLiteral("{{ ext(\"/a/b/c.txt\") }}"), none), QStringLiteral("txt"));
    EXPECT_EQ(render(QStringLiteral("{{ dirname(\"/a/b/c.txt\") }}"), none), QStringLiteral("/a/b"));
    EXPECT_EQ(render(QStringLiteral("{{ lower(\"ABC\") }}"), none), QStringLiteral("abc"));
    EXPECT_EQ(render(QStringLiteral("{{ upper(\"abc\") }}"), none), QStringLiteral("ABC"));
}

TEST(ContextRendererTest, ParseErrorReturnsEmptyAndReportsError)
{
    QString error;
    const QString out = render(QStringLiteral("{{ "), Bindings{}, &error);
    EXPECT_TRUE(out.isEmpty());
    EXPECT_FALSE(error.isEmpty());
}

TEST(ContextRendererTest, ReadsBundledQrcResource)
{
    Q_INIT_RESOURCE(agents);

    QString error;
    const QString out = render(
        QStringLiteral("{{ read_file(\":/roles/qt-cpp-developer.md\") }}"), Bindings{}, &error);

    EXPECT_TRUE(error.isEmpty()) << error.toStdString();
    EXPECT_FALSE(out.trimmed().isEmpty())
        << "read_file(\":/roles/qt-cpp-developer.md\") returned empty — qrc alias broken?";
    EXPECT_TRUE(out.contains(QStringLiteral("Qt/C++ developer")));
}

TEST(ContextRendererTest, SelectsCompletionRoleByLanguageFromQrc)
{
    Q_INIT_RESOURCE(agents);

    const QString tpl = QStringLiteral(
        "{%- if language == \"qml\" %}{{ read_file(\":/roles/code-completion-qml.md\") }}"
        "{%- else if language == \"c-like\" %}{{ read_file(\":/roles/code-completion-c-like.md\") "
        "}}"
        "{%- else %}{{ read_file(\":/roles/code-completion.md\") }}"
        "{%- endif %}");

    Bindings qml;
    qml.language = QStringLiteral("qml");
    Bindings clike;
    clike.language = QStringLiteral("c-like");
    Bindings other;
    other.language = QStringLiteral("python");

    EXPECT_TRUE(render(tpl, qml).contains(QStringLiteral("QML and Qt Quick")));
    EXPECT_TRUE(render(tpl, clike).contains(QStringLiteral("C++, Qt, and QML")));
    EXPECT_TRUE(render(tpl, other).contains(QStringLiteral("expert code completion assistant")));
}
