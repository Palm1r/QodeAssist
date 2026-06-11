// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <AgentConfig.hpp>
#include <AgentLoader.hpp>

using QodeAssist::AgentConfig;
using QodeAssist::Agents::AgentLoader;

namespace {

void writeFile(const QString &dir, const QString &name, const QByteArray &contents)
{
    QFile f(dir + QLatin1Char('/') + name);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(contents);
}

QByteArray minimalAgent(const QByteArray &name, const QByteArray &extra = {})
{
    return "name = \"" + name + "\"\n"
           "provider_instance = \"P\"\n"
           "model = \"m\"\n"
           "endpoint = \"/e\"\n"
           + extra +
           "[body]\n"
           "stream = true\n";
}

const AgentConfig *findConfig(const AgentLoader::LoadResult &result, const QString &name)
{
    for (const auto &cfg : result.configs) {
        if (cfg.name == name)
            return &cfg;
    }
    return nullptr;
}

bool anyContains(const QStringList &list, const QString &needle)
{
    for (const QString &s : list) {
        if (s.contains(needle))
            return true;
    }
    return false;
}

} // namespace

TEST(AgentLoaderTest, WarnsOnUnknownTopLevelAndMatchKeys)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path(), "a.toml",
              minimalAgent("A",
                           "enable_thinkin = true\n"
                           "[match]\n"
                           "file_pattern = [\"*.cpp\"]\n"));

    const auto result = AgentLoader::load(QString(), dir.path());
    EXPECT_TRUE(result.errors.isEmpty()) << result.errors.join("; ").toStdString();
    EXPECT_TRUE(anyContains(result.warnings, QStringLiteral("enable_thinkin")));
    EXPECT_TRUE(anyContains(result.warnings, QStringLiteral("match.file_pattern")));
}

TEST(AgentLoaderTest, WarnsOnDuplicateNameInSameLayer)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path(), "first.toml", minimalAgent("Dup"));
    writeFile(dir.path(), "second.toml", minimalAgent("Dup"));

    const auto result = AgentLoader::load(QString(), dir.path());
    EXPECT_TRUE(anyContains(result.warnings, QStringLiteral("defined in both")));
    const AgentConfig *cfg = findConfig(result, QStringLiteral("Dup"));
    ASSERT_NE(cfg, nullptr);
    EXPECT_TRUE(cfg->sourcePath.endsWith(QStringLiteral("second.toml")));
}

TEST(AgentLoaderTest, UserAgentCollidingWithBundledNameIsRejected)
{
    QTemporaryDir bundled;
    QTemporaryDir user;
    ASSERT_TRUE(bundled.isValid());
    ASSERT_TRUE(user.isValid());
    writeFile(bundled.path(), "a.toml", minimalAgent("A", "description = \"base\"\n"));
    writeFile(user.path(), "a.toml", minimalAgent("A", "description = \"mine\"\n"));

    const auto result = AgentLoader::load(bundled.path(), user.path());
    EXPECT_TRUE(anyContains(result.errors, QStringLiteral("cannot be replaced")));
    const AgentConfig *cfg = findConfig(result, QStringLiteral("A"));
    ASSERT_NE(cfg, nullptr);
    EXPECT_EQ(cfg->description, QStringLiteral("base"));
    EXPECT_FALSE(cfg->isUserSource());
}

TEST(AgentLoaderTest, HiddenIsNotInherited)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path(), "parent.toml", minimalAgent("Parent", "hidden = true\n"));
    writeFile(dir.path(), "child.toml", minimalAgent("Child", "extends = \"Parent\"\n"));

    const auto result = AgentLoader::load(QString(), dir.path());
    EXPECT_TRUE(result.errors.isEmpty()) << result.errors.join("; ").toStdString();
    const AgentConfig *parent = findConfig(result, QStringLiteral("Parent"));
    const AgentConfig *child = findConfig(result, QStringLiteral("Child"));
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);
    EXPECT_TRUE(parent->hidden);
    EXPECT_FALSE(child->hidden);
}

TEST(AgentLoaderTest, UserAgentExtendsBundledProviderBase)
{
    QTemporaryDir bundled;
    QTemporaryDir user;
    ASSERT_TRUE(bundled.isValid());
    ASSERT_TRUE(user.isValid());
    writeFile(bundled.path(), "base.toml",
              "name = \"Provider Base\"\n"
              "abstract = true\n"
              "provider_instance = \"P\"\n"
              "endpoint = \"/e\"\n"
              "[body]\n"
              "stream = true\n");
    writeFile(bundled.path(), "a.toml",
              "name = \"A\"\n"
              "extends = \"Provider Base\"\n"
              "model = \"stock-model\"\n");
    writeFile(user.path(), "mine.toml",
              "name = \"My A\"\n"
              "extends = \"Provider Base\"\n"
              "model = \"my-model\"\n"
              "[body]\n"
              "temperature = 0.2\n");

    const auto result = AgentLoader::load(bundled.path(), user.path());
    EXPECT_TRUE(result.errors.isEmpty()) << result.errors.join("; ").toStdString();
    const AgentConfig *stock = findConfig(result, QStringLiteral("A"));
    ASSERT_NE(stock, nullptr);
    EXPECT_EQ(stock->model, QStringLiteral("stock-model"));
    const AgentConfig *mine = findConfig(result, QStringLiteral("My A"));
    ASSERT_NE(mine, nullptr);
    EXPECT_EQ(mine->model, QStringLiteral("my-model"));
    EXPECT_EQ(mine->providerInstance, QStringLiteral("P"));
    EXPECT_TRUE(mine->body.contains(QStringLiteral("stream")));
    EXPECT_TRUE(mine->body.contains(QStringLiteral("temperature")));
    EXPECT_TRUE(mine->isUserSource());
}

TEST(AgentLoaderTest, ExtendsUnknownParentErrorNamesChild)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path(), "child.toml", minimalAgent("Child", "extends = \"NoSuchBase\"\n"));

    const auto result = AgentLoader::load(QString(), dir.path());
    EXPECT_TRUE(anyContains(result.errors, QStringLiteral("'Child' extends unknown agent 'NoSuchBase'")));
    EXPECT_EQ(findConfig(result, QStringLiteral("Child")), nullptr);
}

TEST(AgentLoaderTest, ParseFileReportsWarningsForOwnFileOnly)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    writeFile(dir.path(), "other.toml", minimalAgent("Other", "bogus_key = 1\n"));
    writeFile(dir.path(), "target.toml", minimalAgent("Target", "another_bogus = 2\n"));

    QString error;
    QStringList warnings;
    const auto cfg = AgentLoader::parseFile(
        dir.path() + QStringLiteral("/target.toml"), QString(), &error, &warnings);
    ASSERT_TRUE(cfg.has_value()) << error.toStdString();
    EXPECT_TRUE(anyContains(warnings, QStringLiteral("another_bogus")));
    EXPECT_FALSE(anyContains(warnings, QStringLiteral("bogus_key")));
}
