// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QString>

#include <AgentConfig.hpp>
#include <AgentLoader.hpp>
#include <JsonPromptTemplate.hpp>

using QodeAssist::Agents::AgentLoader;
using QodeAssist::Templates::JsonPromptTemplate;

TEST(BundledAgentsTest, AllBundledAgentsLoadResolveAndRender)
{
    Q_INIT_RESOURCE(agents);

    const AgentLoader::LoadResult result = AgentLoader::load(QStringLiteral(":/agents"), QString());

    EXPECT_TRUE(result.errors.isEmpty())
        << "bundled agent load errors: "
        << result.errors.join(QStringLiteral("; ")).toStdString();

    EXPECT_TRUE(result.warnings.isEmpty())
        << "bundled agent load warnings: "
        << result.warnings.join(QStringLiteral("; ")).toStdString();

    ASSERT_FALSE(result.configs.empty()) << "no bundled agents were loaded from :/agents";

    for (const auto &cfg : result.configs) {
        QString error;
        const auto tmpl = JsonPromptTemplate::fromConfig(cfg, &error);
        EXPECT_NE(tmpl, nullptr) << "bundled agent '" << cfg.name.toStdString()
                                 << "' body failed to render: " << error.toStdString();
    }
}
