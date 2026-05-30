// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QJsonObject>

#include <AgentConfig.hpp>

using QodeAssist::AgentConfig;

namespace {

AgentConfig makeValid()
{
    AgentConfig cfg;
    cfg.name = QStringLiteral("Agent");
    cfg.providerInstance = QStringLiteral("P");
    cfg.model = QStringLiteral("m");
    cfg.endpoint = QStringLiteral("/e");
    cfg.body = QJsonObject{{"stream", true}};
    return cfg;
}

} // namespace

TEST(AgentConfigTest, ValidConfigReturnsNoError)
{
    EXPECT_TRUE(AgentConfig::validate(makeValid()).isEmpty());
}

TEST(AgentConfigTest, MissingNameRejected)
{
    AgentConfig cfg = makeValid();
    cfg.name.clear();
    EXPECT_TRUE(AgentConfig::validate(cfg).contains(QStringLiteral("no name")));
}

TEST(AgentConfigTest, MissingProviderInstanceRejected)
{
    AgentConfig cfg = makeValid();
    cfg.providerInstance.clear();
    EXPECT_TRUE(AgentConfig::validate(cfg).contains(QStringLiteral("provider_instance")));
}

TEST(AgentConfigTest, MissingModelRejected)
{
    AgentConfig cfg = makeValid();
    cfg.model.clear();
    EXPECT_TRUE(AgentConfig::validate(cfg).contains(QStringLiteral("no model")));
}

TEST(AgentConfigTest, MissingEndpointRejected)
{
    AgentConfig cfg = makeValid();
    cfg.endpoint.clear();
    EXPECT_TRUE(AgentConfig::validate(cfg).contains(QStringLiteral("no endpoint")));
}

TEST(AgentConfigTest, MissingBodyRejected)
{
    AgentConfig cfg = makeValid();
    cfg.body = QJsonObject{};
    EXPECT_TRUE(AgentConfig::validate(cfg).contains(QStringLiteral("[body]")));
}

TEST(AgentConfigTest, FutureSchemaVersionRejected)
{
    AgentConfig cfg = makeValid();
    cfg.schemaVersion = AgentConfig::kSupportedSchemaVersion + 1;
    EXPECT_TRUE(AgentConfig::validate(cfg).contains(QStringLiteral("schema_version")));
}

TEST(AgentConfigTest, SupportedSchemaVersionAccepted)
{
    AgentConfig cfg = makeValid();
    cfg.schemaVersion = AgentConfig::kSupportedSchemaVersion;
    EXPECT_TRUE(AgentConfig::validate(cfg).isEmpty());
}

TEST(AgentConfigTest, MatchIsEmptyWhenAllDimensionsEmpty)
{
    AgentConfig::Match m;
    EXPECT_TRUE(m.isEmpty());
}

TEST(AgentConfigTest, MatchIsNotEmptyWhenAnyDimensionSet)
{
    AgentConfig::Match files;
    files.filePatterns = {QStringLiteral("*.cpp")};
    EXPECT_FALSE(files.isEmpty());

    AgentConfig::Match paths;
    paths.pathPatterns = {QStringLiteral("*/src/*")};
    EXPECT_FALSE(paths.isEmpty());

    AgentConfig::Match projects;
    projects.projectNames = {QStringLiteral("P")};
    EXPECT_FALSE(projects.isEmpty());
}

TEST(AgentConfigTest, IsUserSourceFalseForBundledResourcePath)
{
    AgentConfig cfg = makeValid();
    cfg.sourcePath = QStringLiteral(":/agents/claude_chat.toml");
    EXPECT_FALSE(cfg.isUserSource());
}

TEST(AgentConfigTest, IsUserSourceTrueForFilesystemPath)
{
    AgentConfig cfg = makeValid();
    cfg.sourcePath = QStringLiteral("/home/me/.config/agents/mine.toml");
    EXPECT_TRUE(cfg.isUserSource());
}
