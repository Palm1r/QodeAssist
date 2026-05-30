// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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
    Utils::BoolAspect cancelOnInput{this};

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

    // Context Settings
    Utils::BoolAspect readFullFile{this};
    Utils::BoolAspect readFileParts{this};
    Utils::IntegerAspect readStringsBeforeCursor{this};
    Utils::IntegerAspect readStringsAfterCursor{this};
    Utils::BoolAspect useProjectChangesCache{this};
    Utils::IntegerAspect maxChangesCacheSize{this};

private:
    void setupConnections();
    void resetSettingsToDefaults();
    void migrateCompletionMode();
};

CodeCompletionSettings &codeCompletionSettings();

} // namespace QodeAssist::Settings
