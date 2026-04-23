// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
    Utils::SelectionAspect modelOutputHandler{this};
    Utils::SelectionAspect completionTriggerMode{this};
    Utils::SelectionAspect completionMode{this};
    Utils::BoolAspect smartContextTrigger{this};
    Utils::BoolAspect respectQtcPopup{this};

    Utils::IntegerAspect startSuggestionTimer{this};
    Utils::IntegerAspect autoCompletionCharThreshold{this};
    Utils::IntegerAspect autoCompletionTypingInterval{this};
    Utils::IntegerAspect hintCharThreshold{this};
    Utils::IntegerAspect hintHideTimeout{this};
    Utils::SelectionAspect hintTriggerKey{this};
    Utils::BoolAspect ignoreWhitespaceInCharCount{this};

    Utils::StringListAspect customLanguages{this};

    Utils::BoolAspect showProgressWidget{this};
    Utils::BoolAspect abortAssistOnRequest{this};
    Utils::BoolAspect useOpenFilesContext{this};

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

    // OpenAI Responses API Settings
    Utils::SelectionAspect openAIResponsesReasoningEffort{this};

    QString processMessageToFIM(const QString &prefix, const QString &suffix) const;

private:
    void setupConnections();
    void resetSettingsToDefaults();
    void migrateCompletionMode();
};

CodeCompletionSettings &codeCompletionSettings();

} // namespace QodeAssist::Settings
