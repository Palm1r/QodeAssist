// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utils/aspects.h>

#include "AgentRole.hpp"
#include "ButtonAspect.hpp"

namespace QodeAssist::Settings {

class ChatAssistantSettings : public Utils::AspectContainer
{
public:
    ChatAssistantSettings();

    ButtonAspect resetToDefaults{this};

    // Chat settings
    Utils::IntegerAspect chatTokensThreshold{this};
    Utils::BoolAspect linkOpenFiles{this};
    Utils::BoolAspect autosave{this};
    Utils::BoolAspect enableChatInBottomToolBar{this};
    Utils::BoolAspect enableChatInNavigationPanel{this};
    Utils::BoolAspect enableChatTools{this};

    // General Parameters Settings
    Utils::DoubleAspect temperature{this};
    Utils::IntegerAspect maxTokens{this};

    // Advanced Parameters
    Utils::BoolAspect useTopP{this};
    Utils::DoubleAspect topP{this};

    Utils::BoolAspect useTopK{this};
    Utils::IntegerAspect topK{this};

    Utils::BoolAspect usePresencePenalty{this};
    Utils::DoubleAspect presencePenalty{this};

    Utils::BoolAspect useFrequencyPenalty{this};
    Utils::DoubleAspect frequencyPenalty{this};

    // Context Settings
    Utils::BoolAspect useSystemPrompt{this};
    Utils::StringAspect systemPrompt{this};

    // Ollama Settings
    Utils::StringAspect ollamaLivetime{this};
    Utils::IntegerAspect contextWindow{this};

    // Extended Thinking Settings (Claude only)
    Utils::BoolAspect enableThinkingMode{this};
    Utils::IntegerAspect thinkingBudgetTokens{this};
    Utils::IntegerAspect thinkingMaxTokens{this};

    // OpenAI Responses API Settings
    Utils::SelectionAspect openAIResponsesReasoningEffort{this};

    // Visuals settings
    Utils::SelectionAspect textFontFamily{this};
    Utils::IntegerAspect textFontSize{this};
    Utils::SelectionAspect codeFontFamily{this};
    Utils::IntegerAspect codeFontSize{this};
    Utils::SelectionAspect textFormat{this};

    Utils::SelectionAspect chatRenderer{this};

    Utils::StringAspect lastUsedRoleId{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

ChatAssistantSettings &chatAssistantSettings();

} // namespace QodeAssist::Settings
