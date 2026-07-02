// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

#include <AgentLoader.hpp>
#include <PipelinesConfig.hpp>

using QodeAssist::Agents::AgentLoader;
using QodeAssist::Settings::PipelineRosters;
using QodeAssist::Settings::PipelinesConfig;
using QodeAssist::Settings::PipelinesLoadStatus;

namespace {

class PipelinesConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("pipelines.toml"));
        PipelinesConfig::setFilePathForTests(m_path);
    }

    void TearDown() override { PipelinesConfig::setFilePathForTests(QString()); }

    QTemporaryDir m_dir;
    QString m_path;
};

void expectEqualRosters(const PipelineRosters &a, const PipelineRosters &b)
{
    EXPECT_EQ(a.codeCompletion, b.codeCompletion);
    EXPECT_EQ(a.chatAssistant, b.chatAssistant);
    EXPECT_EQ(a.chatCompression, b.chatCompression);
    EXPECT_EQ(a.quickRefactor, b.quickRefactor);
}

} // namespace

TEST_F(PipelinesConfigTest, DefaultsFillEverySlot)
{
    const PipelineRosters defs = PipelineRosters::defaults();
    EXPECT_FALSE(defs.codeCompletion.isEmpty());
    EXPECT_FALSE(defs.chatAssistant.isEmpty());
    EXPECT_FALSE(defs.chatCompression.isEmpty());
    EXPECT_FALSE(defs.quickRefactor.isEmpty());
}

TEST_F(PipelinesConfigTest, DefaultsReferenceBundledAgents)
{
    Q_INIT_RESOURCE(agents);

    const AgentLoader::LoadResult result = AgentLoader::load(QStringLiteral(":/agents"), QString());
    ASSERT_FALSE(result.configs.empty());

    QStringList bundledNames;
    for (const auto &cfg : result.configs)
        bundledNames.append(cfg.name);

    const PipelineRosters defs = PipelineRosters::defaults();
    QStringList referenced = defs.codeCompletion + defs.chatAssistant;
    referenced << defs.chatCompression << defs.quickRefactor;
    for (const QString &name : std::as_const(referenced)) {
        EXPECT_TRUE(bundledNames.contains(name))
            << "default roster references unknown agent '" << name.toStdString() << "'";
    }
}

TEST_F(PipelinesConfigTest, MissingFileYieldsDefaults)
{
    const auto result = PipelinesConfig::load();
    EXPECT_EQ(result.status, PipelinesLoadStatus::FileMissing);
    expectEqualRosters(result.rosters, PipelineRosters::defaults());
}

TEST_F(PipelinesConfigTest, SaveLoadRoundTrip)
{
    PipelineRosters rosters;
    rosters.codeCompletion = {QStringLiteral("Agent A"), QStringLiteral("Agent B")};
    rosters.chatAssistant = {QStringLiteral("Agent C")};
    rosters.chatCompression = QStringLiteral("Agent D");
    rosters.quickRefactor = QString();

    ASSERT_TRUE(PipelinesConfig::save(rosters));

    const auto result = PipelinesConfig::load();
    EXPECT_EQ(result.status, PipelinesLoadStatus::Ok);
    expectEqualRosters(result.rosters, rosters);
}

TEST_F(PipelinesConfigTest, ExplicitlyEmptySlotStaysEmpty)
{
    PipelineRosters rosters;
    ASSERT_TRUE(PipelinesConfig::save(rosters));

    const auto result = PipelinesConfig::load();
    EXPECT_EQ(result.status, PipelinesLoadStatus::Ok);
    EXPECT_TRUE(result.rosters.codeCompletion.isEmpty());
    EXPECT_TRUE(result.rosters.chatAssistant.isEmpty());
    EXPECT_TRUE(result.rosters.chatCompression.isEmpty());
    EXPECT_TRUE(result.rosters.quickRefactor.isEmpty());
}

TEST_F(PipelinesConfigTest, LegacyArrayCollapsesToFirstName)
{
    QFile f(m_path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(
        "[pipelines]\n"
        "code_completion = [\"CC\"]\n"
        "chat_assistant = [\"CA\"]\n"
        "chat_compression = [\"First\", \"Second\"]\n"
        "quick_refactor = [\"Only\"]\n");
    f.close();

    const auto result = PipelinesConfig::load();
    EXPECT_EQ(result.status, PipelinesLoadStatus::Ok);
    EXPECT_EQ(result.rosters.chatCompression, QStringLiteral("First"));
    EXPECT_EQ(result.rosters.quickRefactor, QStringLiteral("Only"));
}

TEST_F(PipelinesConfigTest, SaveInvalidatesCache)
{
    PipelineRosters first;
    first.chatCompression = QStringLiteral("Before");
    ASSERT_TRUE(PipelinesConfig::save(first));
    EXPECT_EQ(PipelinesConfig::loadCached().rosters.chatCompression, QStringLiteral("Before"));

    PipelineRosters second;
    second.chatCompression = QStringLiteral("AfterX");
    ASSERT_TRUE(PipelinesConfig::save(second));
    EXPECT_EQ(PipelinesConfig::loadCached().rosters.chatCompression, QStringLiteral("AfterX"));
}
