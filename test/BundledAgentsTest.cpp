// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QString>

#include <AgentConfig.hpp>
#include <AgentLoader.hpp>
#include <ContextRenderer.hpp>
#include <JsonPromptTemplate.hpp>

using QodeAssist::Agents::AgentLoader;
using QodeAssist::Templates::JsonPromptTemplate;
using QodeAssist::Templates::ContextRenderer::Bindings;
using QodeAssist::Templates::ContextRenderer::render;

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

TEST(BundledAgentsTest, AllBundledSystemPromptsResolveQrcResources)
{
    Q_INIT_RESOURCE(agents);

    const AgentLoader::LoadResult result = AgentLoader::load(QStringLiteral(":/agents"), QString());
    ASSERT_TRUE(result.errors.isEmpty())
        << result.errors.join(QStringLiteral("; ")).toStdString();
    ASSERT_FALSE(result.configs.empty());

    const QStringList languages = {QString(), QStringLiteral("qml"), QStringLiteral("c-like")};

    int withSystemPrompt = 0;
    for (const auto &cfg : result.configs) {
        if (cfg.systemPrompt.isEmpty())
            continue;
        ++withSystemPrompt;
        for (const QString &lang : languages) {
            Bindings bindings;
            bindings.language = lang;

            QString error;
            const QString rendered = render(cfg.systemPrompt, bindings, &error);

            EXPECT_TRUE(error.isEmpty())
                << "agent '" << cfg.name.toStdString() << "' (language='"
                << lang.toStdString() << "') system_prompt render error: " << error.toStdString();
            EXPECT_FALSE(rendered.trimmed().isEmpty())
                << "agent '" << cfg.name.toStdString() << "' (language='" << lang.toStdString()
                << "') system_prompt rendered empty — a read_file(\":/...\") path is likely broken";
        }
    }

    EXPECT_GT(withSystemPrompt, 0)
        << "no bundled agent carried a system_prompt — this test would be vacuous";
}
