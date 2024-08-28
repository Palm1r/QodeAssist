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

#include <QPushButton>
#include <utils/aspects.h>
#include <utils/layoutbuilder.h>

namespace QodeAssist {

template<typename AspectType>
void resetAspect(AspectType &aspect)
{
    aspect.setValue(aspect.defaultValue());
}

class ButtonAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    ButtonAspect(Utils::AspectContainer *container = nullptr)
        : Utils::BaseAspect(container)
    {}

    void addToLayout(Layouting::Layout &parent) override
    {
        auto button = new QPushButton(m_buttonText);
        connect(button, &QPushButton::clicked, this, &ButtonAspect::clicked);
        parent.addItem(button);
    }

    QString m_buttonText;
signals:
    void clicked();
};

class QodeAssistSettings : public Utils::AspectContainer
{
public:
    QodeAssistSettings();

    Utils::BoolAspect enableQodeAssist{this};
    Utils::BoolAspect enableAutoComplete{this};
    Utils::BoolAspect enableLogging{this};

    Utils::SelectionAspect llmProviders{this};
    Utils::StringAspect url{this};
    Utils::IntegerAspect port{this};
    Utils::StringAspect endPoint{this};

    Utils::StringAspect modelName{this};
    ButtonAspect selectModels{this};

    Utils::SelectionAspect fimPrompts{this};
    Utils::DoubleAspect temperature{this};
    Utils::IntegerAspect maxTokens{this};

    Utils::BoolAspect readFullFile{this};
    Utils::IntegerAspect readStringsBeforeCursor{this};
    Utils::IntegerAspect readStringsAfterCursor{this};

    Utils::BoolAspect useTopP{this};
    Utils::DoubleAspect topP{this};

    Utils::BoolAspect useTopK{this};
    Utils::DoubleAspect topK{this};

    Utils::BoolAspect usePresencePenalty{this};
    Utils::DoubleAspect presencePenalty{this};

    Utils::BoolAspect useFrequencyPenalty{this};
    Utils::DoubleAspect frequencyPenalty{this};

    Utils::StringListAspect providerPaths{this};

    Utils::IntegerAspect startSuggestionTimer{this};
    Utils::IntegerAspect maxFileThreshold{this};

    Utils::StringAspect ollamaLivetime{this};
    Utils::StringAspect specificInstractions{this};

    ButtonAspect resetToDefaults{this};

private:
    void setupConnections();
    void updateProviderSettings();
    QStringList getInstalledModels();
    void showModelSelectionDialog();
    Utils::Environment getEnvironmentWithProviderPaths() const;
    void resetSettingsToDefaults();
};

QodeAssistSettings &settings();

} // namespace QodeAssist
