/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

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

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

QuickRefactorSettings &quickRefactorSettings();

} // namespace QodeAssist::Settings

