// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/TurnContext.hpp"

namespace QodeAssist::Session {

QString renderSystemPrompt(const TurnContext &context)
{
    QString prompt = context.basePrompt;

    if (context.rolePrompt)
        prompt += "\n\n" + *context.rolePrompt;

    if (context.project.available) {
        prompt += QString("\n# Active project: %1").arg(context.project.displayName);
        prompt += QString(
                      "\n# Project source root: %1"
                      "\n#   All new source files, headers, QML and CMake edits MUST be "
                      "created or modified under this directory. Use absolute paths "
                      "rooted here, or project-relative paths.")
                      .arg(context.project.sourceRoot);

        if (context.project.buildDirectory) {
            prompt += QString(
                          "\n# Build output directory (compiler artifacts only — do NOT "
                          "create or edit source files here): %1")
                          .arg(*context.project.buildDirectory);
        }

        if (!context.projectRules.isEmpty())
            prompt += QString("\n# Project Rules\n\n") + context.projectRules;
    } else {
        prompt += QString("\n# No active project in IDE");
    }

    if (!context.alwaysOnSkills.isEmpty())
        prompt += QString("\n\n") + context.alwaysOnSkills;

    if (!context.skillsCatalog.isEmpty())
        prompt += QString("\n\n") + context.skillsCatalog;

    for (const InvokedSkill &skill : context.invokedSkills)
        prompt += QString("\n\n# Invoked Skill: %1\n\n%2").arg(skill.name, skill.body);

    return prompt;
}

} // namespace QodeAssist::Session
