// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace QodeAssist::Settings {

class QuickRefactorSettings : public Utils::AspectContainer
{
public:
    QuickRefactorSettings();

    ButtonAspect resetToDefaults{this};

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

    // Ollama Settings
    Utils::StringAspect ollamaLivetime{this};
    Utils::IntegerAspect contextWindow{this};

    // Tools Settings
    Utils::BoolAspect useTools{this};

    // Thinking Settings
    Utils::BoolAspect useThinking{this};
    Utils::IntegerAspect thinkingBudgetTokens{this};
    Utils::IntegerAspect thinkingMaxTokens{this};

    // OpenAI Responses API Settings
    Utils::SelectionAspect openAIResponsesReasoningEffort{this};

    // Context Settings
    Utils::BoolAspect readFullFile{this};
    Utils::BoolAspect readFileParts{this};
    Utils::IntegerAspect readStringsBeforeCursor{this};
    Utils::IntegerAspect readStringsAfterCursor{this};

    // Display Settings
    Utils::SelectionAspect displayMode{this};
    Utils::SelectionAspect widgetOrientation{this};
    Utils::IntegerAspect widgetMinWidth{this};
    Utils::IntegerAspect widgetMaxWidth{this};
    Utils::IntegerAspect widgetMinHeight{this};
    Utils::IntegerAspect widgetMaxHeight{this};

    // Prompt Settings
    Utils::StringAspect systemPrompt{this};
    Utils::BoolAspect useOpenFilesInQuickRefactor{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

QuickRefactorSettings &quickRefactorSettings();

} // namespace QodeAssist::Settings

