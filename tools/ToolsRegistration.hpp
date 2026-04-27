// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace LLMQore {
class ToolsManager;
}

namespace QodeAssist::Tools {

void registerQodeAssistTools(::LLMQore::ToolsManager *manager);

} // namespace QodeAssist::Tools
