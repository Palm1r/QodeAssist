// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

namespace LLMQore {
class ToolsManager;
}

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Tools {

void registerQodeAssistTools(::LLMQore::ToolsManager *manager);

void registerSkillTool(
    ::LLMQore::ToolsManager *manager, Skills::SkillsManager *skillsManager);

} // namespace QodeAssist::Tools
