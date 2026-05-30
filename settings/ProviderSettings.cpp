// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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

LegacyApiKeyEntry legacyApiKeyForClientApi(const QString &clientApi)
{
    ProviderSettings &s = providerSettings();
    QString label;
    QString value;

    if (clientApi == "Claude") {
        label = QStringLiteral("Claude");
        value = s.claudeApiKey();
    } else if (clientApi == "OpenRouter") {
        label = QStringLiteral("OpenRouter");
        value = s.openRouterApiKey();
    } else if (clientApi == "OpenAI Compatible") {
        label = QStringLiteral("OpenAI Compatible");
        value = s.openAiCompatApiKey();
    } else if (clientApi == "OpenAI (Chat Completions)" || clientApi == "OpenAI (Responses API)") {
        label = QStringLiteral("OpenAI");
        value = s.openAiApiKey();
    } else if (clientApi == "Mistral AI") {
        label = QStringLiteral("Mistral AI");
        value = s.mistralAiApiKey();
    } else if (clientApi == "Codestral") {
        label = QStringLiteral("Codestral");
        value = s.codestralApiKey();
    } else if (clientApi == "Google AI") {
        label = QStringLiteral("Google AI");
        value = s.googleAiApiKey();
    } else if (clientApi == "Ollama (Native)" || clientApi == "Ollama (OpenAI-compatible)") {
        label = QStringLiteral("Ollama (Bearer)");
        value = s.ollamaBasicAuthApiKey();
    } else if (clientApi == "llama.cpp") {
        label = QStringLiteral("llama.cpp");
        value = s.llamaCppApiKey();
    }

    if (value.isEmpty())
        return {};
    return {label, value};
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

    claudeEnablePromptCaching.setSettingsKey(Constants::CLAUDE_ENABLE_PROMPT_CACHING);
    claudeEnablePromptCaching.setLabelText(Tr::tr("Enable prompt caching"));
    claudeEnablePromptCaching.setToolTip(
        Tr::tr("Marks the system prompt, tool definitions, and stable chat history with "
               "cache_control so Anthropic caches the request prefix (5-minute TTL). "
               "Reduces cost and latency on repeated turns."));
    claudeEnablePromptCaching.setDefaultValue(false);
    claudeEnablePromptCaching.setAutoApply(true);

    claudeUseExtendedCacheTTL.setSettingsKey(Constants::CLAUDE_USE_EXTENDED_CACHE_TTL);
    claudeUseExtendedCacheTTL.setLabelText(Tr::tr("Use 1h cache TTL (beta)"));
    claudeUseExtendedCacheTTL.setToolTip(
        Tr::tr("Requests Anthropic's 1-hour cache TTL instead of the default 5 minutes. "
               "Sends the extended-cache-ttl-2025-04-11 beta header."));
    claudeUseExtendedCacheTTL.setDefaultValue(false);
    claudeUseExtendedCacheTTL.setAutoApply(true);

    // OpenAI Settings
    openAiApiKey.setSettingsKey(Constants::OPEN_AI_API_KEY);
    openAiApiKey.setLabelText(Tr::tr("OpenAI API Key:"));
    openAiApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    openAiApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    openAiApiKey.setHistoryCompleter(Constants::OPEN_AI_API_KEY_HISTORY);
    openAiApiKey.setDefaultValue("");
    openAiApiKey.setAutoApply(true);

    // MistralAI Settings
    mistralAiApiKey.setSettingsKey(Constants::MISTRAL_AI_API_KEY);
    mistralAiApiKey.setLabelText(Tr::tr("Mistral AI API Key:"));
    mistralAiApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    mistralAiApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    mistralAiApiKey.setHistoryCompleter(Constants::MISTRAL_AI_API_KEY_HISTORY);
    mistralAiApiKey.setDefaultValue("");
    mistralAiApiKey.setAutoApply(true);

    codestralApiKey.setSettingsKey(Constants::CODESTRAL_API_KEY);
    codestralApiKey.setLabelText(Tr::tr("Codestral API Key:"));
    codestralApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    codestralApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    codestralApiKey.setHistoryCompleter(Constants::CODESTRAL_API_KEY_HISTORY);
    codestralApiKey.setDefaultValue("");
    codestralApiKey.setAutoApply(true);

    // GoogleAI Settings
    googleAiApiKey.setSettingsKey(Constants::GOOGLE_AI_API_KEY);
    googleAiApiKey.setLabelText(Tr::tr("Google AI API Key:"));
    googleAiApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    googleAiApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    googleAiApiKey.setHistoryCompleter(Constants::GOOGLE_AI_API_KEY_HISTORY);
    googleAiApiKey.setDefaultValue("");
    googleAiApiKey.setAutoApply(true);

    // Ollama with BasicAuth Settings
    ollamaBasicAuthApiKey.setSettingsKey(Constants::OLLAMA_BASIC_AUTH_API_KEY);
    ollamaBasicAuthApiKey.setLabelText(Tr::tr("Ollama(Bearer) API Key:"));
    ollamaBasicAuthApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    ollamaBasicAuthApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    ollamaBasicAuthApiKey.setHistoryCompleter(Constants::OLLAMA_BASIC_AUTH_API_KEY_HISTORY);
    ollamaBasicAuthApiKey.setDefaultValue("");
    ollamaBasicAuthApiKey.setAutoApply(true);

    // llama.cpp Settings
    llamaCppApiKey.setSettingsKey(Constants::LLAMA_CPP_API_KEY);
    llamaCppApiKey.setLabelText(Tr::tr("llama.cpp API Key:"));
    llamaCppApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    llamaCppApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    llamaCppApiKey.setHistoryCompleter(Constants::LLAMA_CPP_API_KEY_HISTORY);
    llamaCppApiKey.setDefaultValue("");
    llamaCppApiKey.setAutoApply(true);

    // Qwen (Alibaba) Settings
    qwenApiKey.setSettingsKey(Constants::QWEN_API_KEY);
    qwenApiKey.setLabelText(Tr::tr("Qwen API Key:"));
    qwenApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    qwenApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    qwenApiKey.setHistoryCompleter(Constants::QWEN_API_KEY_HISTORY);
    qwenApiKey.setDefaultValue("");
    qwenApiKey.setAutoApply(true);

    // DeepSeek Settings
    deepSeekApiKey.setSettingsKey(Constants::DEEPSEEK_API_KEY);
    deepSeekApiKey.setLabelText(Tr::tr("DeepSeek API Key:"));
    deepSeekApiKey.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    deepSeekApiKey.setPlaceHolderText(Tr::tr("Enter your API key here"));
    deepSeekApiKey.setHistoryCompleter(Constants::DEEPSEEK_API_KEY_HISTORY);
    deepSeekApiKey.setDefaultValue("");
    deepSeekApiKey.setAutoApply(true);

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
            Group{title(Tr::tr("OpenAI Settings")), Column{openAiApiKey}},
            Space{8},
            Group{title(Tr::tr("OpenAI Compatible Settings")), Column{openAiCompatApiKey}},
            Space{8},
            Group{
                title(Tr::tr("Claude Settings")),
                Column{claudeApiKey, claudeEnablePromptCaching, claudeUseExtendedCacheTTL}},
            Space{8},
            Group{title(Tr::tr("Mistral AI Settings")), Column{mistralAiApiKey, codestralApiKey}},
            Space{8},
            Group{title(Tr::tr("Google AI Settings")), Column{googleAiApiKey}},
            Space{8},
            Group{title(Tr::tr("Ollama Settings")), Column{ollamaBasicAuthApiKey}},
            Space{8},
            Group{title(Tr::tr("llama.cpp Settings")), Column{llamaCppApiKey}},
            Space{8},
            Group{title(Tr::tr("Qwen (Alibaba) Settings")), Column{qwenApiKey}},
            Space{8},
            Group{title(Tr::tr("DeepSeek Settings")), Column{deepSeekApiKey}},
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
    connect(&claudeEnablePromptCaching, &Utils::BoolAspect::changed, this, [this]() {
        claudeEnablePromptCaching.writeSettings();
    });
    connect(&claudeUseExtendedCacheTTL, &Utils::BoolAspect::changed, this, [this]() {
        claudeUseExtendedCacheTTL.writeSettings();
    });
    connect(&openAiApiKey, &ButtonAspect::changed, this, [this]() { openAiApiKey.writeSettings(); });
    connect(&mistralAiApiKey, &ButtonAspect::changed, this, [this]() {
        mistralAiApiKey.writeSettings();
    });
    connect(&codestralApiKey, &ButtonAspect::changed, this, [this]() {
        codestralApiKey.writeSettings();
    });
    connect(&googleAiApiKey, &ButtonAspect::changed, this, [this]() {
        googleAiApiKey.writeSettings();
    });
    connect(&ollamaBasicAuthApiKey, &ButtonAspect::changed, this, [this]() {
        ollamaBasicAuthApiKey.writeSettings();
    });
    connect(&llamaCppApiKey, &ButtonAspect::changed, this, [this]() {
        llamaCppApiKey.writeSettings();
    });
    connect(&qwenApiKey, &ButtonAspect::changed, this, [this]() { qwenApiKey.writeSettings(); });
    connect(&deepSeekApiKey, &ButtonAspect::changed, this, [this]() {
        deepSeekApiKey.writeSettings();
    });
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
        resetAspect(claudeEnablePromptCaching);
        resetAspect(claudeUseExtendedCacheTTL);
        resetAspect(openAiApiKey);
        resetAspect(mistralAiApiKey);
        resetAspect(googleAiApiKey);
        resetAspect(ollamaBasicAuthApiKey);
        resetAspect(llamaCppApiKey);
        resetAspect(qwenApiKey);
        resetAspect(deepSeekApiKey);
        writeSettings();
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

} // namespace QodeAssist::Settings
