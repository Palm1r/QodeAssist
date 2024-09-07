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

#include "QodeAssistSettings.hpp"

#include <QFileDialog>
#include <QInputDialog>
#include <QJsonParseError>
#include <QtWidgets/qmessagebox.h>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"

#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistUtils.hpp"

namespace QodeAssist {

QodeAssistSettings &settings()
{
    static QodeAssistSettings settings;
    return settings;
}

QodeAssistSettings::QodeAssistSettings()
{
    setAutoApply(false);

    readSettings();

    setLayouter([this]() {
        using namespace Layouting;

        return Column{Group{title(Tr::tr("FIM Prompt Settings")),
                            Form{Column{Column{
                                               },
                                        // readFullFile,
                                        // maxFileThreshold,
                                        // readStringsBeforeCursor,
                                        // readStringsAfterCursor,
                                        // ollamaLivetime,
                                        // apiKey,
                                        // useFilePathInContext,
                                        // useSpecificInstructions,
                                        // specificInstractions,
                                        // temperature,
                                        // maxTokens,
                                        // startSuggestionTimer,
                                        // Row{useTopP, topP, Stretch{1}},
                                        // Row{useTopK, topK, Stretch{1}},
/*                                        Row{usePresencePenalty, presencePenalty, Stretch{1}},
                                        Row{useFrequencyPenalty, frequencyPenalty, Stretch{1}}*/}}},
                      st};
    });

    setupConnections();
}

void QodeAssistSettings::setupConnections()
{

}

QStringList QodeAssistSettings::getInstalledModels()
{
    auto *provider = LLMProvidersManager::instance().getCurrentProvider();
    if (provider) {
        Utils::Environment env = Utils::Environment::systemEnvironment();
        return provider->getInstalledModels(env);
    }
    return {};
}

void QodeAssistSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // resetAspect(enableQodeAssist);
        // resetAspect(enableAutoComplete);
        // resetAspect(llmProviders);
        // resetAspect(url);
        // resetAspect(endPoint);
        // resetAspect(modelName);
        // resetAspect(fimPrompts);
        // resetAspect(temperature);
        // resetAspect(maxTokens);
        // resetAspect(readFullFile);
        // resetAspect(maxFileThreshold);
        // resetAspect(readStringsBeforeCursor);
        // resetAspect(readStringsAfterCursor);
        // resetAspect(useTopP);
        // resetAspect(topP);
        // resetAspect(useTopK);
        // resetAspect(topK);
        // resetAspect(usePresencePenalty);
        // resetAspect(presencePenalty);
        // resetAspect(useFrequencyPenalty);
        // resetAspect(frequencyPenalty);
        // resetAspect(startSuggestionTimer);
        // resetAspect(enableLogging);
        // resetAspect(ollamaLivetime);
        // resetAspect(specificInstractions);
        // resetAspect(multiLineCompletion);
        // resetAspect(useFilePathInContext);
        // resetAspect(useSpecificInstructions);
        // resetAspect(customJsonTemplate);

        // fimPrompts.setStringValue("StarCoder2");
        // llmProviders.setStringValue("Ollama");

        apply();

        QMessageBox::information(Core::ICore::dialogParent(),
                                 Tr::tr("Settings Reset"),
                                 Tr::tr("All settings have been reset to their default values."));
    }
}

void QodeAssistSettings::saveCustomTemplate()
{
    // QString fileName = QFileDialog::getSaveFileName(nullptr,
    //                                                 Tr::tr("Save JSON Template"),
    //                                                 QString(),
    //                                                 Tr::tr("JSON Files (*.json)"));
    // if (fileName.isEmpty())
    //     return;

    // QFile file(fileName);
    // if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    //     QTextStream out(&file);
    //     out << customJsonTemplate.value();
    //     file.close();
    //     QMessageBox::information(nullptr,
    //                              Tr::tr("Save Successful"),
    //                              Tr::tr("JSON template has been saved successfully."));
    // } else {
    //     QMessageBox::critical(nullptr,
    //                           Tr::tr("Save Failed"),
    //                           Tr::tr("Failed to save JSON template."));
    // }
}

void QodeAssistSettings::loadCustomTemplate()
{
    // QString fileName = QFileDialog::getOpenFileName(nullptr,
    //                                                 Tr::tr("Load JSON Template"),
    //                                                 QString(),
    //                                                 Tr::tr("JSON Files (*.json)"));
    // if (fileName.isEmpty())
    //     return;

    // QFile file(fileName);
    // if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    //     QTextStream in(&file);
    //     QString jsonContent = in.readAll();
    //     file.close();

    //     QJsonParseError parseError;
    //     QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseError);
    //     if (parseError.error == QJsonParseError::NoError) {
    //         customJsonTemplate.setValue(jsonContent);
    //         QMessageBox::information(nullptr,
    //                                  Tr::tr("Load Successful"),
    //                                  Tr::tr("JSON template has been loaded successfully."));
    //     } else {
    //         QMessageBox::critical(nullptr,
    //                               Tr::tr("Invalid JSON"),
    //                               Tr::tr("The selected file contains invalid JSON."));
    //     }
    // } else {
    //     QMessageBox::critical(nullptr,
    //                           Tr::tr("Load Failed"),
    //                           Tr::tr("Failed to load JSON template."));
    // }
}

class QodeAssistSettingsPage : public Core::IOptionsPage
{
public:
    QodeAssistSettingsPage()
    {
        setId(Constants::QODE_ASSIST_GENERAL_OPTIONS_ID);
        setDisplayName("Qode Assist");
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setDisplayCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY);
        setCategoryIconPath(":/resources/images/qoderassist-icon.png");
        setSettingsProvider([] { return &settings(); });
    }
};

const QodeAssistSettingsPage settingsPage;

} // namespace QodeAssist
