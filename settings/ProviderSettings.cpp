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

#include "ProviderSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <QMessageBox>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

ProviderSettings &providerSettings()
{
    static ProviderSettings settings;
    return settings;
}

ProviderSettings::ProviderSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Provider Settings"));

    // OpenRouter Settings
    openRouterApiKey.setSettingsKey(Constants::OPEN_ROUTER_API_KEY);
    openRouterApiKey.setLabelText(Tr::tr("OpenRouter API Key:"));
    openRouterApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    openRouterApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    openRouterApiKey.setHistoryCompleter(Constants::OPEN_ROUTER_API_KEY_HISTORY);
    openRouterApiKey.setDefaultValue("");
    openRouterApiKey.setAutoApply(true);

    // OpenAI Compatible Settings
    openAiCompatApiKey.setSettingsKey(Constants::OPEN_AI_COMPAT_API_KEY);
    openAiCompatApiKey.setLabelText(Tr::tr("OpenAI Compatible API Key:"));
    openAiCompatApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    openAiCompatApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    openAiCompatApiKey.setHistoryCompleter(Constants::OPEN_AI_COMPAT_API_KEY_HISTORY);
    openAiCompatApiKey.setDefaultValue("");
    openAiCompatApiKey.setAutoApply(true);

    // Claude Compatible Settings
    claudeApiKey.setSettingsKey(Constants::CLAUDE_API_KEY);
    claudeApiKey.setLabelText(Tr::tr("Claude API Key:"));
    claudeApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    claudeApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    claudeApiKey.setHistoryCompleter(Constants::CLAUDE_API_KEY_HISTORY);
    claudeApiKey.setDefaultValue("");
    claudeApiKey.setAutoApply(true);

    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    readSettings();

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{title(Tr::tr("OpenRouter Settings")), Column{openRouterApiKey}},
            Space{8},
            Group{title(Tr::tr("OpenAI Compatible Settings")), Column{openAiCompatApiKey}},
            Space{8},
            Group{title(Tr::tr("Claude Settings")), Column{claudeApiKey}},
            Stretch{1}};
    });
}

void ProviderSettings::setupConnections()
{
    connect(
        &resetToDefaults, &ButtonAspect::clicked, this, &ProviderSettings::resetSettingsToDefaults);
    connect(&openRouterApiKey, &ButtonAspect::changed, this, [this]() {
        openRouterApiKey.writeSettings();
    });
    connect(&openAiCompatApiKey, &ButtonAspect::changed, this, [this]() {
        openAiCompatApiKey.writeSettings();
    });
    connect(&claudeApiKey, &ButtonAspect::changed, this, [this]() { claudeApiKey.writeSettings(); });
}

void ProviderSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(openRouterApiKey);
        resetAspect(openAiCompatApiKey);
        resetAspect(claudeApiKey);
    }
}

class ProviderSettingsPage : public Core::IOptionsPage
{
public:
    ProviderSettingsPage()
    {
        setId(Constants::QODE_ASSIST_PROVIDER_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Provider Settings"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &providerSettings(); });
    }
};

const ProviderSettingsPage providerSettingsPage;

} // namespace QodeAssist::Settings