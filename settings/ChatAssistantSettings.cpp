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

#include "ChatAssistantSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <QApplication>
#include <QFontDatabase>
#include <QMessageBox>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

ChatAssistantSettings &chatAssistantSettings()
{
    static ChatAssistantSettings settings;
    return settings;
}

ChatAssistantSettings::ChatAssistantSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Chat Assistant"));

    // Chat Settings
    chatTokensThreshold.setSettingsKey(Constants::CA_TOKENS_THRESHOLD);
    chatTokensThreshold.setLabelText(Tr::tr("Chat history token limit:"));
    chatTokensThreshold.setToolTip(Tr::tr("Maximum number of tokens in chat history. When "
                                          "exceeded, oldest messages will be removed."));
    chatTokensThreshold.setRange(1, 99999999);
    chatTokensThreshold.setDefaultValue(20000);

    linkOpenFiles.setSettingsKey(Constants::CA_LINK_OPEN_FILES);
    linkOpenFiles.setLabelText(Tr::tr("Sync open files with assistant by default"));
    linkOpenFiles.setDefaultValue(false);

    autosave.setSettingsKey(Constants::CA_AUTOSAVE);
    autosave.setDefaultValue(true);
    autosave.setLabelText(Tr::tr("Enable autosave when message received"));

    enableChatInBottomToolBar.setSettingsKey(Constants::CA_ENABLE_CHAT_IN_BOTTOM_TOOLBAR);
    enableChatInBottomToolBar.setLabelText(Tr::tr("Enable chat in bottom toolbar"));
    enableChatInBottomToolBar.setDefaultValue(false);

    enableChatInNavigationPanel.setSettingsKey(Constants::CA_ENABLE_CHAT_IN_NAVIGATION_PANEL);
    enableChatInNavigationPanel.setLabelText(Tr::tr("Enable chat in navigation panel"));
    enableChatInNavigationPanel.setDefaultValue(false);


    // General Parameters Settings
    temperature.setSettingsKey(Constants::CA_TEMPERATURE);
    temperature.setLabelText(Tr::tr("Temperature:"));
    temperature.setDefaultValue(0.5);
    temperature.setRange(0.0, 2.0);
    temperature.setSingleStep(0.1);

    maxTokens.setSettingsKey(Constants::CA_MAX_TOKENS);
    maxTokens.setLabelText(Tr::tr("Max Tokens:"));
    maxTokens.setRange(-1, 10000);
    maxTokens.setDefaultValue(2000);

    // Advanced Parameters
    useTopP.setSettingsKey(Constants::CA_USE_TOP_P);
    useTopP.setDefaultValue(false);
    useTopP.setLabelText(Tr::tr("Top P:"));

    topP.setSettingsKey(Constants::CA_TOP_P);
    topP.setDefaultValue(0.9);
    topP.setRange(0.0, 1.0);
    topP.setSingleStep(0.1);

    useTopK.setSettingsKey(Constants::CA_USE_TOP_K);
    useTopK.setDefaultValue(false);
    useTopK.setLabelText(Tr::tr("Top K:"));

    topK.setSettingsKey(Constants::CA_TOP_K);
    topK.setDefaultValue(50);
    topK.setRange(1, 1000);

    usePresencePenalty.setSettingsKey(Constants::CA_USE_PRESENCE_PENALTY);
    usePresencePenalty.setDefaultValue(false);
    usePresencePenalty.setLabelText(Tr::tr("Presence Penalty:"));

    presencePenalty.setSettingsKey(Constants::CA_PRESENCE_PENALTY);
    presencePenalty.setDefaultValue(0.0);
    presencePenalty.setRange(-2.0, 2.0);
    presencePenalty.setSingleStep(0.1);

    useFrequencyPenalty.setSettingsKey(Constants::CA_USE_FREQUENCY_PENALTY);
    useFrequencyPenalty.setDefaultValue(false);
    useFrequencyPenalty.setLabelText(Tr::tr("Frequency Penalty:"));

    frequencyPenalty.setSettingsKey(Constants::CA_FREQUENCY_PENALTY);
    frequencyPenalty.setDefaultValue(0.0);
    frequencyPenalty.setRange(-2.0, 2.0);
    frequencyPenalty.setSingleStep(0.1);

    // Context Settings
    useSystemPrompt.setSettingsKey(Constants::CA_USE_SYSTEM_PROMPT);
    useSystemPrompt.setDefaultValue(true);
    useSystemPrompt.setLabelText(Tr::tr("Use System Prompt"));

    systemPrompt.setSettingsKey(Constants::CA_SYSTEM_PROMPT);
    systemPrompt.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    systemPrompt.setDefaultValue(
        "You are an advanced AI assistant specializing in C++, Qt, and QML development. Your role "
        "is to provide helpful, accurate, and detailed responses to questions about coding, "
        "debugging, "
        "and best practices in these technologies.");

    // Ollama Settings
    ollamaLivetime.setSettingsKey(Constants::CA_OLLAMA_LIVETIME);
    ollamaLivetime.setToolTip(
        Tr::tr("Time to suspend Ollama after completion request (in minutes), "
               "Only Ollama,  -1 to disable"));
    ollamaLivetime.setLabelText("Livetime:");
    ollamaLivetime.setDefaultValue("5m");
    ollamaLivetime.setDisplayStyle(Utils::StringAspect::LineEditDisplay);

    contextWindow.setSettingsKey(Constants::CA_OLLAMA_CONTEXT_WINDOW);
    contextWindow.setLabelText(Tr::tr("Context Window:"));
    contextWindow.setRange(-1, 10000);
    contextWindow.setDefaultValue(2048);

    autosave.setDefaultValue(true);
    autosave.setLabelText(Tr::tr("Enable autosave when message received"));

    textFontFamily.setSettingsKey(Constants::CA_TEXT_FONT_FAMILY);
    textFontFamily.setLabelText(Tr::tr("Text Font:"));
    textFontFamily.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        textFontFamily.addOption(family);
    }
    textFontFamily.setDefaultValue(QApplication::font().family());

    textFontSize.setSettingsKey(Constants::CA_TEXT_FONT_SIZE);
    textFontSize.setLabelText(Tr::tr("Text Font Size:"));
    textFontSize.setDefaultValue(QApplication::font().pointSize());

    codeFontFamily.setSettingsKey(Constants::CA_CODE_FONT_FAMILY);
    codeFontFamily.setLabelText(Tr::tr("Code Font:"));
    codeFontFamily.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    const QStringList monospaceFamilies = QFontDatabase::families(QFontDatabase::Latin);
    for (const QString &family : monospaceFamilies) {
        if (QFontDatabase::isFixedPitch(family)) {
            codeFontFamily.addOption(family);
        }
    }

    QString defaultMonoFont;
    QStringList fixedPitchFamilies;

    for (const QString &family : monospaceFamilies) {
        if (QFontDatabase::isFixedPitch(family)) {
            fixedPitchFamilies.append(family);
        }
    }

    if (fixedPitchFamilies.contains("Consolas")) {
        defaultMonoFont = "Consolas";
    } else if (fixedPitchFamilies.contains("Courier New")) {
        defaultMonoFont = "Courier New";
    } else if (fixedPitchFamilies.contains("Monospace")) {
        defaultMonoFont = "Monospace";
    } else if (!fixedPitchFamilies.isEmpty()) {
        defaultMonoFont = fixedPitchFamilies.first();
    } else {
        defaultMonoFont = QApplication::font().family();
    }
    codeFontFamily.setDefaultValue(defaultMonoFont);
    codeFontSize.setSettingsKey(Constants::CA_CODE_FONT_SIZE);
    codeFontSize.setLabelText(Tr::tr("Code Font Size:"));
    codeFontSize.setDefaultValue(QApplication::font().pointSize());

    textFormat.setSettingsKey(Constants::CA_TEXT_FORMAT);
    textFormat.setLabelText(Tr::tr("Text Format:"));
    textFormat.setDefaultValue(0);
    textFormat.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    textFormat.addOption("Markdown");
    textFormat.addOption("HTML");
    textFormat.addOption("Plain Text");

    chatRenderer.setSettingsKey(Constants::CA_CHAT_RENDERER);
    chatRenderer.setLabelText(Tr::tr("Chat Renderer:"));
    chatRenderer.addOption("rhi");
    chatRenderer.addOption("software");
    chatRenderer.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
#ifdef Q_OS_WIN
    chatRenderer.setDefaultValue("software");
#else
    chatRenderer.setDefaultValue("rhi");
#endif

    resetToDefaults.m_buttonText = TrConstants::RESET_TO_DEFAULTS;

    readSettings();

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto genGrid = Grid{};
        genGrid.addRow({Row{temperature}});
        genGrid.addRow({Row{maxTokens}});

        auto advancedGrid = Grid{};
        advancedGrid.addRow({useTopP, topP});
        advancedGrid.addRow({useTopK, topK});
        advancedGrid.addRow({usePresencePenalty, presencePenalty});
        advancedGrid.addRow({useFrequencyPenalty, frequencyPenalty});

        auto ollamaGrid = Grid{};
        ollamaGrid.addRow({ollamaLivetime});
        ollamaGrid.addRow({contextWindow});

        auto chatViewSettingsGrid = Grid{};
        chatViewSettingsGrid.addRow({textFontFamily, textFontSize});
        chatViewSettingsGrid.addRow({codeFontFamily, codeFontSize});
        chatViewSettingsGrid.addRow({textFormat});
        chatViewSettingsGrid.addRow({chatRenderer});

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{
                title(Tr::tr("Chat Settings")),
                Column{
                    Row{chatTokensThreshold, Stretch{1}},
                    linkOpenFiles,
                    autosave,
                    enableChatInBottomToolBar,
                    enableChatInNavigationPanel}},
            Space{8},
            Group{
                title(Tr::tr("General Parameters")),
                Row{genGrid, Stretch{1}},
            },
            Space{8},
            Group{title(Tr::tr("Advanced Parameters")), Column{Row{advancedGrid, Stretch{1}}}},
            Space{8},
            Group{
                title(Tr::tr("Context Settings")),
                Column{
                    Row{useSystemPrompt, Stretch{1}},
                    systemPrompt,
                }},
            Group{title(Tr::tr("Ollama Settings")), Column{Row{ollamaGrid, Stretch{1}}}},
            Group{title(Tr::tr("Chat Settings")), Row{chatViewSettingsGrid, Stretch{1}}},
            Stretch{1}};
    });
}

void ChatAssistantSettings::setupConnections()
{
    connect(
        &resetToDefaults,
        &ButtonAspect::clicked,
        this,
        &ChatAssistantSettings::resetSettingsToDefaults);
}

void ChatAssistantSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(chatTokensThreshold);
        resetAspect(temperature);
        resetAspect(maxTokens);
        resetAspect(useTopP);
        resetAspect(topP);
        resetAspect(useTopK);
        resetAspect(topK);
        resetAspect(usePresencePenalty);
        resetAspect(presencePenalty);
        resetAspect(useFrequencyPenalty);
        resetAspect(frequencyPenalty);
        resetAspect(useSystemPrompt);
        resetAspect(systemPrompt);
        resetAspect(ollamaLivetime);
        resetAspect(contextWindow);
        resetAspect(linkOpenFiles);
        resetAspect(textFontFamily);
        resetAspect(codeFontFamily);
        resetAspect(textFontSize);
        resetAspect(codeFontSize);
        resetAspect(textFormat);
        resetAspect(chatRenderer);
    }
}

class ChatAssistantSettingsPage : public Core::IOptionsPage
{
public:
    ChatAssistantSettingsPage()
    {
        setId(Constants::QODE_ASSIST_CHAT_ASSISTANT_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Chat Assistant"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &chatAssistantSettings(); });
    }
};

const ChatAssistantSettingsPage chatAssistantSettingsPage;

} // namespace QodeAssist::Settings
