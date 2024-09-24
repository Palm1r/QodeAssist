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

#include "GeneralSettings.hpp"

#include <QInputDialog>
#include <QMessageBox>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <utils/utilsicons.h>

#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistConstants.hpp"
#include "QodeAssistUtils.hpp"
#include "QodeAssisttr.h"

namespace QodeAssist::Settings {

GeneralSettings &generalSettings()
{
    static GeneralSettings settings;
    return settings;
}

GeneralSettings::GeneralSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("General"));

    enableQodeAssist.setSettingsKey(Constants::ENABLE_QODE_ASSIST);
    enableQodeAssist.setLabelText(Tr::tr("Enable Qode Assist"));
    enableQodeAssist.setDefaultValue(true);

    enableAutoComplete.setSettingsKey(Constants::ENABLE_AUTO_COMPLETE);
    enableAutoComplete.setLabelText(Tr::tr("Enable Auto Complete"));
    enableAutoComplete.setDefaultValue(true);

    enableLogging.setSettingsKey(Constants::ENABLE_LOGGING);
    enableLogging.setLabelText(Tr::tr("Enable Logging"));
    enableLogging.setDefaultValue(false);

    multiLineCompletion.setSettingsKey(Constants::MULTILINE_COMPLETION);
    multiLineCompletion.setDefaultValue(false);
    multiLineCompletion.setLabelText(Tr::tr("Enable Multiline Completion(experimental)"));

    startSuggestionTimer.setSettingsKey(Constants::START_SUGGESTION_TIMER);
    startSuggestionTimer.setLabelText(Tr::tr("with delay(ms)"));
    startSuggestionTimer.setRange(10, 10000);
    startSuggestionTimer.setDefaultValue(500);

    autoCompletionCharThreshold.setSettingsKey(Constants::AUTO_COMPLETION_CHAR_THRESHOLD);
    autoCompletionCharThreshold.setLabelText(Tr::tr("AI suggestion triggers after typing"));
    autoCompletionCharThreshold.setToolTip(
        Tr::tr("The number of characters that need to be typed within the typing interval "
               "before an AI suggestion request is sent."));
    autoCompletionCharThreshold.setRange(0, 10);
    autoCompletionCharThreshold.setDefaultValue(0);

    autoCompletionTypingInterval.setSettingsKey(Constants::AUTO_COMPLETION_TYPING_INTERVAL);
    autoCompletionTypingInterval.setLabelText(Tr::tr("character(s) within(ms)"));
    autoCompletionTypingInterval.setToolTip(
        Tr::tr("The time window (in milliseconds) during which the character threshold "
               "must be met to trigger an AI suggestion request."));
    autoCompletionTypingInterval.setRange(500, 5000);
    autoCompletionTypingInterval.setDefaultValue(2000);

    llmProviders.setSettingsKey(Constants::LLM_PROVIDERS);
    llmProviders.setDisplayName(Tr::tr("AI Suggest Provider:"));
    llmProviders.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);

    url.setSettingsKey(Constants::URL);
    url.setLabelText(Tr::tr("URL:"));
    url.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    endPoint.setSettingsKey(Constants::END_POINT);
    endPoint.setLabelText(Tr::tr("FIM Endpoint:"));
    endPoint.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    modelName.setSettingsKey(Constants::MODEL_NAME);
    modelName.setLabelText(Tr::tr("Model name:"));
    modelName.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    selectModels.m_buttonText = Tr::tr("Select Fill-In-the-Middle Model");

    fimPrompts.setDisplayName(Tr::tr("Fill-In-the-Middle Prompt"));
    fimPrompts.setSettingsKey(Constants::FIM_PROMPTS);
    fimPrompts.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    resetToDefaults.m_buttonText = Tr::tr("Reset Page to Defaults");

    chatLlmProviders.setSettingsKey(Constants::CHAT_LLM_PROVIDERS);
    chatLlmProviders.setDisplayName(Tr::tr("AI Chat Provider:"));
    chatLlmProviders.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);

    chatUrl.setSettingsKey(Constants::CHAT_URL);
    chatUrl.setLabelText(Tr::tr("URL:"));
    chatUrl.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    chatEndPoint.setSettingsKey(Constants::CHAT_END_POINT);
    chatEndPoint.setLabelText(Tr::tr("Chat Endpoint:"));
    chatEndPoint.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    chatModelName.setSettingsKey(Constants::CHAT_MODEL_NAME);
    chatModelName.setLabelText(Tr::tr("Model name:"));
    chatModelName.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    chatSelectModels.m_buttonText = Tr::tr("Select Chat Model");

    chatPrompts.setDisplayName(Tr::tr("Chat Prompt"));
    chatPrompts.setSettingsKey(Constants::CHAT_PROMPTS);
    chatPrompts.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);

    loadProviders();
    loadPrompts();

    readSettings();

    auto fimProviderName = llmProviders.displayForIndex(llmProviders.value());
    setCurrentFimProvider(fimProviderName);
    auto chatProviderName = chatLlmProviders.displayForIndex(chatLlmProviders.value());
    setCurrentChatProvider(chatProviderName);

    auto nameFimPromts = fimPrompts.displayForIndex(fimPrompts.value());
    PromptTemplateManager::instance().setCurrentFimTemplate(nameFimPromts);
    auto nameChatPromts = chatPrompts.displayForIndex(chatPrompts.value());
    PromptTemplateManager::instance().setCurrentChatTemplate(nameChatPromts);

    setLoggingEnabled(enableLogging());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto rootLayout
            = Column{Row{enableQodeAssist, Stretch{1}, resetToDefaults},
                     enableAutoComplete,
                     multiLineCompletion,
                     Row{autoCompletionCharThreshold,
                         autoCompletionTypingInterval,
                         startSuggestionTimer,
                         Stretch{1}},
                     Space{8},
                     enableLogging,
                     Space{8},
                     Group{title(Tr::tr("AI Suggestions")),
                           Column{Row{llmProviders, Stretch{1}},
                                  Row{url, endPoint, fimUrlIndicator},
                                  Row{selectModels, modelName, fimModelIndicator},
                                  Row{fimPrompts, Stretch{1}}}},
                     Space{16},
                     Group{title(Tr::tr("AI Chat(experimental)")),
                           Column{Row{chatLlmProviders, Stretch{1}},
                                  Row{chatUrl, chatEndPoint, chatUrlIndicator},
                                  Row{chatSelectModels, chatModelName, chatModelIndicator},
                                  Row{chatPrompts, Stretch{1}}}},
                     Stretch{1}};
        return rootLayout;
    });

    updateStatusIndicators();
}

void GeneralSettings::setupConnections()
{
    connect(&llmProviders, &Utils::SelectionAspect::volatileValueChanged, this, [this]() {
        auto providerName = llmProviders.displayForIndex(llmProviders.volatileValue());
        setCurrentFimProvider(providerName);
        modelName.setVolatileValue("");
    });
    connect(&chatLlmProviders, &Utils::SelectionAspect::volatileValueChanged, this, [this]() {
        auto providerName = chatLlmProviders.displayForIndex(chatLlmProviders.volatileValue());
        setCurrentChatProvider(providerName);
        chatModelName.setVolatileValue("");        
    });

    connect(&fimPrompts, &Utils::SelectionAspect::volatileValueChanged, this, [this]() {
        int index = fimPrompts.volatileValue();
        PromptTemplateManager::instance().setCurrentFimTemplate(fimPrompts.displayForIndex(index));
    });
    connect(&chatPrompts, &Utils::SelectionAspect::volatileValueChanged, this, [this]() {
        int index = chatPrompts.volatileValue();
        PromptTemplateManager::instance().setCurrentChatTemplate(chatPrompts.displayForIndex(index));
    });

    connect(&selectModels, &ButtonAspect::clicked, this, [this]() {
        auto *provider = LLMProvidersManager::instance().getCurrentFimProvider();
        showModelSelectionDialog(&modelName, provider);
    });
    connect(&chatSelectModels, &ButtonAspect::clicked, this, [this]() {
        auto *provider = LLMProvidersManager::instance().getCurrentChatProvider();
        showModelSelectionDialog(&chatModelName, provider);
    });

    connect(&enableLogging, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        setLoggingEnabled(enableLogging.volatileValue());
    });
    connect(&resetToDefaults, &ButtonAspect::clicked, this, &GeneralSettings::resetPageToDefaults);

    connect(&url,
            &Utils::StringAspect::volatileValueChanged,
            this,
            &GeneralSettings::updateStatusIndicators);
    connect(&modelName,
            &Utils::StringAspect::volatileValueChanged,
            this,
            &GeneralSettings::updateStatusIndicators);
    connect(&chatUrl,
            &Utils::StringAspect::volatileValueChanged,
            this,
            &GeneralSettings::updateStatusIndicators);
    connect(&chatModelName,
            &Utils::StringAspect::volatileValueChanged,
            this,
            &GeneralSettings::updateStatusIndicators);
}

void GeneralSettings::showModelSelectionDialog(Utils::StringAspect *modelNameObj,
                                               Providers::LLMProvider *provider)
{
    Utils::Environment env = Utils::Environment::systemEnvironment();
    QString providerUrl = (modelNameObj == &modelName) ? url() : chatUrl();

    if (provider) {
        QStringList models = provider->getInstalledModels(env, providerUrl);
        bool ok;
        QString selectedModel = QInputDialog::getItem(Core::ICore::dialogParent(),
                                                      Tr::tr("Select LLM Model"),
                                                      Tr::tr("Choose a model:"),
                                                      models,
                                                      0,
                                                      false,
                                                      &ok);

        if (ok && !selectedModel.isEmpty()) {
            modelNameObj->setVolatileValue(selectedModel);
            writeSettings();
        }
    }
}

void GeneralSettings::resetPageToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(enableQodeAssist);
        resetAspect(enableAutoComplete);
        resetAspect(enableLogging);
        resetAspect(startSuggestionTimer);
        resetAspect(autoCompletionTypingInterval);
        resetAspect(autoCompletionCharThreshold);
    }

    int fimIndex = llmProviders.indexForDisplay("Ollama");
    llmProviders.setVolatileValue(fimIndex);
    int chatIndex = chatLlmProviders.indexForDisplay("Ollama");
    chatLlmProviders.setVolatileValue(chatIndex);
    modelName.setVolatileValue("");
    chatModelName.setVolatileValue("");

    updateStatusIndicators();
}

void GeneralSettings::updateStatusIndicators()
{
    bool fimUrlValid = !url.volatileValue().isEmpty() && !endPoint.volatileValue().isEmpty();
    bool fimModelValid = !modelName.volatileValue().isEmpty();
    bool chatUrlValid = !chatUrl.volatileValue().isEmpty()
                        && !chatEndPoint.volatileValue().isEmpty();
    bool chatModelValid = !chatModelName.volatileValue().isEmpty();

    bool fimPingSuccessful = false;
    if (fimUrlValid) {
        QUrl pingUrl(url.volatileValue());
        fimPingSuccessful = QodeAssist::pingUrl(pingUrl);
    }
    bool chatPingSuccessful = false;
    if (chatUrlValid) {
        QUrl pingUrl(chatUrl.volatileValue());
        chatPingSuccessful = QodeAssist::pingUrl(pingUrl);
    }

    setIndicatorStatus(fimModelIndicator,
                       fimModelValid ? tr("Model is properly configured")
                                     : tr("No model selected or model name is invalid"),
                       fimModelValid);
    setIndicatorStatus(fimUrlIndicator,
                       fimPingSuccessful ? tr("Server is reachable")
                                         : tr("Server is not reachable or URL is invalid"),
                       fimPingSuccessful);

    setIndicatorStatus(chatModelIndicator,
                       chatModelValid ? tr("Model is properly configured")
                                      : tr("No model selected or model name is invalid"),
                       chatModelValid);
    setIndicatorStatus(chatUrlIndicator,
                       chatPingSuccessful ? tr("Server is reachable")
                                          : tr("Server is not reachable or URL is invalid"),
                       chatPingSuccessful);
}

void GeneralSettings::setIndicatorStatus(Utils::StringAspect &indicator,
                                         const QString &tooltip,
                                         bool isValid)
{
    const Utils::Icon &icon = isValid ? Utils::Icons::OK : Utils::Icons::WARNING;
    indicator.setLabelPixmap(icon.pixmap());
    indicator.setToolTip(tooltip);
}

void GeneralSettings::setCurrentFimProvider(const QString &name)
{
    const auto provider = LLMProvidersManager::instance().setCurrentFimProvider(name);
    if (!provider)
        return;

    url.setValue(provider->url());
    endPoint.setValue(provider->completionEndpoint());
}

void GeneralSettings::setCurrentChatProvider(const QString &name)
{
    const auto provider = LLMProvidersManager::instance().setCurrentChatProvider(name);
    if (!provider)
        return;

    chatUrl.setValue(provider->url());
    chatEndPoint.setValue(provider->chatEndpoint());
}

void GeneralSettings::loadProviders()
{
    for (const auto &name : LLMProvidersManager::instance().providersNames()) {
        llmProviders.addOption(name);
        chatLlmProviders.addOption(name);
    }
}

void GeneralSettings::loadPrompts()
{
    for (const auto &name : PromptTemplateManager::instance().fimTemplatesNames()) {
        fimPrompts.addOption(name);
    }
    for (const auto &name : PromptTemplateManager::instance().chatTemplatesNames()) {
        chatPrompts.addOption(name);
    }
}

class GeneralSettingsPage : public Core::IOptionsPage
{
public:
    GeneralSettingsPage()
    {
        setId(Constants::QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("General"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &generalSettings(); });
    }
};

const GeneralSettingsPage generalSettingsPage;

} // namespace QodeAssist::Settings
