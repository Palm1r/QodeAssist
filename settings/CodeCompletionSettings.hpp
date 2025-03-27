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

    // Auto Completion Settings
    Utils::BoolAspect autoCompletion{this};
    Utils::BoolAspect multiLineCompletion{this};
    Utils::BoolAspect stream{this};
    Utils::BoolAspect smartProcessInstuctText{this};

    Utils::IntegerAspect startSuggestionTimer{this};
    Utils::IntegerAspect autoCompletionCharThreshold{this};
    Utils::IntegerAspect autoCompletionTypingInterval{this};

    Utils::StringListAspect customLanguages{this};

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
    Utils::StringAspect systemPrompt{this};
    Utils::BoolAspect useUserMessageTemplateForCC{this};
    Utils::StringAspect systemPromptForNonFimModels{this};
    Utils::StringAspect userMessageTemplateForCC{this};
    Utils::BoolAspect useProjectChangesCache{this};
    Utils::IntegerAspect maxChangesCacheSize{this};

    // Ollama Settings
    Utils::StringAspect ollamaLivetime{this};
    Utils::IntegerAspect contextWindow{this};

    QString processMessageToFIM(const QString &prefix, const QString &suffix) const;

private:
    void setupConnections();
    void resetSettingsToDefaults();
};

CodeCompletionSettings &codeCompletionSettings();

} // namespace QodeAssist::Settings
