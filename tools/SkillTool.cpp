// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SkillTool.hpp"

#include <LLMQore/ToolExceptions.hpp>

#include <optional>

#include <QJsonArray>
#include <QtConcurrent>

#include "sources/skills/AgentSkill.hpp"
#include "sources/skills/SkillsManager.hpp"

namespace QodeAssist::Tools {

SkillTool::SkillTool(Skills::SkillsManager *skillsManager, QObject *parent)
    : BaseTool(parent)
    , m_skillsManager(skillsManager)
{}

QString SkillTool::id() const
{
    return "load_skill";
}

QString SkillTool::displayName() const
{
    return "Loading skill";
}

QString SkillTool::description() const
{
    return "Load the full instructions of a skill by name. The Available Skills catalog in "
           "the system prompt lists each skill's name and a short description. When a request "
           "matches a skill, call this tool with that skill's name to load its complete "
           "instructions, then follow them.";
}

QJsonObject SkillTool::parametersSchema() const
{
    QJsonObject properties;
    properties["name"] = QJsonObject{
        {"type", "string"},
        {"description",
         "Exact name of the skill to load, as shown in the Available Skills catalog."}};

    QJsonObject definition;
    definition["type"] = "object";
    definition["properties"] = properties;
    definition["required"] = QJsonArray{"name"};
    return definition;
}

QFuture<LLMQore::ToolResult> SkillTool::executeAsync(const QJsonObject &input)
{
    const QString name = input["name"].toString().trimmed();

    const std::optional<Skills::AgentSkill> found
        = m_skillsManager ? m_skillsManager->findByName(name) : std::nullopt;

    return QtConcurrent::run([name, found]() -> LLMQore::ToolResult {
        if (name.isEmpty()) {
            throw LLMQore::ToolInvalidArgument(
                "'name' parameter is required and cannot be empty");
        }
        if (!found) {
            throw LLMQore::ToolRuntimeError(
                QString("Unknown skill: '%1'. Use a skill name from the Available Skills "
                        "catalog in the system prompt.")
                    .arg(name));
        }
        if (found->body.isEmpty()) {
            throw LLMQore::ToolRuntimeError(
                QString("Skill '%1' has no instructions.").arg(found->name));
        }

        return LLMQore::ToolResult::text(
            QString("Skill: %1\n\n%2").arg(found->name, found->body));
    });
}

} // namespace QodeAssist::Tools
