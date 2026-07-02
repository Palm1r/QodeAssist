// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderSettings.hpp"

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"

namespace QodeAssist::Settings {

ProviderSettings &providerSettings()
{
    static ProviderSettings settings;
    return settings;
}

LegacyApiKeyEntry legacyApiKeyForClientApi(const QString &clientApi, const QString &instanceName)
{
    ProviderSettings &s = providerSettings();
    QString label;
    QString value;

    if (instanceName == "Qwen") {
        label = QStringLiteral("Qwen");
        value = s.qwenApiKey();
    } else if (instanceName == "DeepSeek") {
        label = QStringLiteral("DeepSeek");
        value = s.deepSeekApiKey();
    } else if (clientApi == "Claude") {
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
    } else if (clientApi == "Ollama (Native)" || clientApi == "Ollama (OpenAI-compatible)") {
        label = QStringLiteral("Ollama (Bearer)");
        value = s.ollamaBasicAuthApiKey();
    } else if (clientApi == "llama.cpp") {
        label = QStringLiteral("llama.cpp");
        value = s.llamaCppApiKey();
    } else if (clientApi == "Google AI") {
        label = QStringLiteral("Google AI");
        value = s.googleAiApiKey();
    }

    if (value.isEmpty())
        return {};
    return {label, value};
}

ProviderSettings::ProviderSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Provider Settings"));

    openRouterApiKey.setSettingsKey(Constants::OPEN_ROUTER_API_KEY);
    openRouterApiKey.setDefaultValue("");

    openAiCompatApiKey.setSettingsKey(Constants::OPEN_AI_COMPAT_API_KEY);
    openAiCompatApiKey.setDefaultValue("");

    claudeApiKey.setSettingsKey(Constants::CLAUDE_API_KEY);
    claudeApiKey.setDefaultValue("");

    openAiApiKey.setSettingsKey(Constants::OPEN_AI_API_KEY);
    openAiApiKey.setDefaultValue("");

    mistralAiApiKey.setSettingsKey(Constants::MISTRAL_AI_API_KEY);
    mistralAiApiKey.setDefaultValue("");

    codestralApiKey.setSettingsKey(Constants::CODESTRAL_API_KEY);
    codestralApiKey.setDefaultValue("");

    googleAiApiKey.setSettingsKey(Constants::GOOGLE_AI_API_KEY);
    googleAiApiKey.setDefaultValue("");

    ollamaBasicAuthApiKey.setSettingsKey(Constants::OLLAMA_BASIC_AUTH_API_KEY);
    ollamaBasicAuthApiKey.setDefaultValue("");

    llamaCppApiKey.setSettingsKey(Constants::LLAMA_CPP_API_KEY);
    llamaCppApiKey.setDefaultValue("");

    qwenApiKey.setSettingsKey(Constants::QWEN_API_KEY);
    qwenApiKey.setDefaultValue("");

    deepSeekApiKey.setSettingsKey(Constants::DEEPSEEK_API_KEY);
    deepSeekApiKey.setDefaultValue("");

    readSettings();
}

} // namespace QodeAssist::Settings
