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

#include "CustomPromptSettings.hpp"

#include <QFileDialog>
#include <QJsonParseError>
#include <QMessageBox>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>

#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"

namespace QodeAssist::Settings {

CustomPromptSettings &customPromptSettings()
{
    static CustomPromptSettings settings;
    return settings;
}

CustomPromptSettings::CustomPromptSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Custom Prompt"));

    customJsonLabel.setLabelText("Custom JSON Template:");
    customJsonLabel.setDisplayStyle(Utils::StringAspect::LabelDisplay);

    customJsonLegend.setLabelText(Tr::tr(R"(Prompt components:
- model is set on General Page
- {{QODE_INSTRUCTIONS}}: Placeholder for specific instructions or context.
- {{QODE_PREFIX}}: Will be replaced with the actual code before the cursor.
- {{QODE_SUFFIX}}: Will be replaced with the actual code after the cursor.
)"));

    customJsonTemplate.setSettingsKey(Constants::CUSTOM_JSON_TEMPLATE);
    customJsonTemplate.setDisplayStyle(Utils::StringAspect::TextEditDisplay);

    customJsonTemplate.setDefaultValue(R"({
  "prompt": "{{QODE_INSTRUCTIONS}}<fim_prefix>{{QODE_PREFIX}}<fim_suffix>{{QODE_SUFFIX}}<fim_middle>",
  "options": {
    "temperature": 0.7,
    "top_p": 0.95,
    "top_k": 40,
    "num_predict": 100,
    "stop": [
      "<|endoftext|>",
      "<file_sep>",
      "<fim_prefix>",
      "<fim_suffix>",
      "<fim_middle>"
    ],
    "frequency_penalty": 0,
    "presence_penalty": 0
  },
  "stream": true
})");
    saveCustomTemplateButton.m_buttonText = (Tr::tr("Save Custom Template to JSON"));
    loadCustomTemplateButton.m_buttonText = (Tr::tr("Load Custom Template from JSON"));
    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    setupConnection();

    setLayouter([this]() {
        using namespace Layouting;
        return Column{Row{customJsonLabel, Stretch{1}, resetToDefaults},
                      Row{customJsonTemplate,
                          Column{saveCustomTemplateButton,
                                 loadCustomTemplateButton,
                                 customJsonLegend,
                                 Stretch{1}}}};
    });
}

void CustomPromptSettings::setupConnection()
{
    connect(&resetToDefaults,
            &ButtonAspect::clicked,
            this,
            &CustomPromptSettings::resetSettingsToDefaults);
    connect(&saveCustomTemplateButton,
            &ButtonAspect::clicked,
            this,
            &CustomPromptSettings::saveCustomTemplate);
    connect(&loadCustomTemplateButton,
            &ButtonAspect::clicked,
            this,
            &CustomPromptSettings::loadCustomTemplate);
}

void CustomPromptSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(customJsonTemplate);
    }

    QMessageBox::information(Core::ICore::dialogParent(),
                             Tr::tr("Settings Reset"),
                             Tr::tr("All settings have been reset to their default values."));
}

void CustomPromptSettings::saveCustomTemplate()
{
    QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                    Tr::tr("Save JSON Template"),
                                                    QString(),
                                                    Tr::tr("JSON Files (*.json)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << customJsonTemplate.value();
        file.close();
        QMessageBox::information(nullptr,
                                 Tr::tr("Save Successful"),
                                 Tr::tr("JSON template has been saved successfully."));
    } else {
        QMessageBox::critical(nullptr,
                              Tr::tr("Save Failed"),
                              Tr::tr("Failed to save JSON template."));
    }
}

void CustomPromptSettings::loadCustomTemplate()
{
    QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                    Tr::tr("Load JSON Template"),
                                                    QString(),
                                                    Tr::tr("JSON Files (*.json)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString jsonContent = in.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            customJsonTemplate.setVolatileValue(jsonContent);
            QMessageBox::information(nullptr,
                                     Tr::tr("Load Successful"),
                                     Tr::tr("JSON template has been loaded successfully."));
        } else {
            QMessageBox::critical(nullptr,
                                  Tr::tr("Invalid JSON"),
                                  Tr::tr("The selected file contains invalid JSON."));
        }
    } else {
        QMessageBox::critical(nullptr,
                              Tr::tr("Load Failed"),
                              Tr::tr("Failed to load JSON template."));
    }
}

class CustomPromptSettingsPage : public Core::IOptionsPage
{
public:
    CustomPromptSettingsPage()
    {
        setId(Constants::QODE_ASSIST_CUSTOM_PROMPT_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Custom Prompt"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &customPromptSettings(); });
    }
};

const CustomPromptSettingsPage customPromptSettingsPage;

} // namespace QodeAssist::Settings
