/* 
 * Copyright (C) 2024 Petr Mironychev
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

class CodeCompletionSettings : public Utils::AspectContainer
{
public:
    CodeCompletionSettings();

    ButtonAspect resetToDefaults{this};

    Utils::IntegerAspect configVersion{this}; // used to track config version for upgrades

    // Auto Completion Settings
    Utils::BoolAspect autoCompletion{this};
    Utils::BoolAspect multiLineCompletion{this};
    Utils::BoolAspect stream{this};
    Utils::BoolAspect smartProcessInstuctText{this};

    Utils::IntegerAspect startSuggestionTimer{this};
    Utils::IntegerAspect autoCompletionCharThreshold{this};
    Utils::IntegerAspect autoCompletionTypingInterval{this};

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
    Utils::BoolAspect readFullFile{this};
    Utils::BoolAspect readFileParts{this};
    Utils::IntegerAspect readStringsBeforeCursor{this};
    Utils::IntegerAspect readStringsAfterCursor{this};
    Utils::BoolAspect useSystemPrompt{this};
    Utils::StringAspect systemPromptJinja{this};
    Utils::BoolAspect useUserMessageTemplateForCC{this};
    Utils::StringAspect systemPromptForNonFimModelsJinja{this};
    Utils::StringAspect userMessageTemplateForCCjinja{this};
    Utils::BoolAspect useProjectChangesCache{this};
    Utils::IntegerAspect maxChangesCacheSize{this};

    // Ollama Settings
    Utils::StringAspect ollamaLivetime{this};
    Utils::IntegerAspect contextWindow{this};

    // API Configuration Settings
    Utils::StringAspect apiKey{this};

private:
    // The aspects below only exist to fetch configuration from settings produced
    // by old versions of the plugin. This configuration is used to fill in new versions
    // of these settings so that users do not need to upgrade manually.
    Utils::StringAspect systemPrompt{this};
    Utils::StringAspect systemPromptForNonFimModels{this};
    Utils::StringAspect userMessageTemplateForCC{this};

    void setupConnections();
    void resetSettingsToDefaults();
    void upgradeOldTemplatesToJinja();
};

CodeCompletionSettings &codeCompletionSettings();

} // namespace QodeAssist::Settings
