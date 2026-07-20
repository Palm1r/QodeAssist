// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/TurnContextBuilder.hpp"

#include <QRegularExpression>
#include <QStringList>

namespace QodeAssist::Session {

TurnContextBuilder::TurnContextBuilder(
    const IProjectContextPort &project, const ISkillsContextPort *skills)
    : m_project(project)
    , m_skills(skills)
{}

TurnContext TurnContextBuilder::build(const TurnContextRequest &request) const
{
    TurnContext context;
    context.basePrompt = request.basePrompt;
    context.rolePrompt = request.rolePrompt;

    context.project = m_project.projectInfo();
    if (context.project.available && request.needs.systemPrompt)
        context.projectRules = m_project.projectRules();

    if (m_skills && request.needs.systemPrompt) {
        context.alwaysOnSkills = m_skills->alwaysOnBodies();
        context.skillsCatalog = m_skills->catalogText();

        static const QRegularExpression skillCommand(
            QStringLiteral("(?:^|\\s)/([a-z0-9][a-z0-9-]*)"));
        QStringList invokedSkillNames;
        auto skillMatch = skillCommand.globalMatch(request.message);
        while (skillMatch.hasNext()) {
            const QString skillName = skillMatch.next().captured(1);
            if (invokedSkillNames.contains(skillName))
                continue;
            const auto invokedSkill = m_skills->findSkill(skillName);
            if (invokedSkill && !invokedSkill->body.isEmpty()) {
                invokedSkillNames << skillName;
                context.invokedSkills.append(*invokedSkill);
            }
        }
    }

    return context;
}

} // namespace QodeAssist::Session
