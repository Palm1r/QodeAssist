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

#pragma once

namespace QodeAssist::Constants {

// project settings

const char QODE_ASSIST_PROJECT_SETTINGS_ID[] = "QodeAssist.ProjectSettings";
const char QODE_ASSIST_USE_GLOBAL_SETTINGS[] = "QodeAssist.UseGlobalSettings";
const char QODE_ASSIST_ENABLE_IN_PROJECT[] = "QodeAssist.EnableInProject";
const char QODE_ASSIST_CHAT_HISTORY_PATH[] = "QodeAssist.ChatHistoryPath";

// new settings
const char CC_PROVIDER[] = "QodeAssist.ccProvider";
const char CC_MODEL[] = "QodeAssist.ccModel";
const char CC_MODEL_HISTORY[] = "QodeAssist.ccModelHistory";
const char CC_TEMPLATE[] = "QodeAssist.ccTemplate";
const char CC_URL[] = "QodeAssist.ccUrl";
const char CC_URL_HISTORY[] = "QodeAssist.ccUrlHistory";
const char CC_ENDPOINT_MODE[] = "QodeAssist.ccEndpointMode";
const char CC_CUSTOM_ENDPOINT[] = "QodeAssist.ccCustomEndpoint";
const char CC_CUSTOM_ENDPOINT_HISTORY[] = "QodeAssist.ccCustomEndpointHistory";

const char CA_PROVIDER[] = "QodeAssist.caProvider";
const char CA_MODEL[] = "QodeAssist.caModel";
const char CA_MODEL_HISTORY[] = "QodeAssist.caModelHistory";
const char CA_TEMPLATE[] = "QodeAssist.caTemplate";
const char CA_URL[] = "QodeAssist.caUrl";
const char CA_URL_HISTORY[] = "QodeAssist.caUrlHistory";
const char CA_ENDPOINT_MODE[] = "QodeAssist.caEndpointMode";
const char CA_CUSTOM_ENDPOINT[] = "QodeAssist.caCustomEndpoint";
const char CA_CUSTOM_ENDPOINT_HISTORY[] = "QodeAssist.caCustomEndpointHistory";

const char CC_SPECIFY_PRESET1[] = "QodeAssist.ccSpecifyPreset1";
const char CC_PRESET1_LANGUAGE[] = "QodeAssist.ccPreset1Language";
const char CC_PRESET1_PROVIDER[] = "QodeAssist.ccPreset1Provider";
const char CC_PRESET1_MODEL[] = "QodeAssist.ccPreset1Model";
const char CC_PRESET1_MODEL_HISTORY[] = "QodeAssist.ccPreset1ModelHistory";
const char CC_PRESET1_TEMPLATE[] = "QodeAssist.ccPreset1Template";
const char CC_PRESET1_URL[] = "QodeAssist.ccPreset1Url";
const char CC_PRESET1_URL_HISTORY[] = "QodeAssist.ccPreset1UrlHistory";
const char CC_PRESET1_ENDPOINT_MODE[] = "QodeAssist.caPreset1EndpointMode";
const char CC_PRESET1_CUSTOM_ENDPOINT[] = "QodeAssist.caPreset1CustomEndpointHistory";
const char CC_PRESET1_CUSTOM_ENDPOINT_HISTORY[] = "QodeAssist.caPreset1CustomEndpointHistory";

// settings
const char ENABLE_QODE_ASSIST[] = "QodeAssist.enableQodeAssist";
const char CC_AUTO_COMPLETION[] = "QodeAssist.ccAutoCompletion";
const char CC_SHOW_PROGRESS_WIDGET[] = "QodeAssist.ccShowProgressWidget";
const char CC_USE_OPEN_FILES_CONTEXT[] = "QodeAssist.ccUseOpenFilesContext";
const char ENABLE_LOGGING[] = "QodeAssist.enableLogging";
const char ENABLE_CHECK_UPDATE[] = "QodeAssist.enableCheckUpdate";

const char PROVIDER_PATHS[] = "QodeAssist.providerPaths";
const char СС_START_SUGGESTION_TIMER[] = "QodeAssist.startSuggestionTimer";
const char СС_AUTO_COMPLETION_CHAR_THRESHOLD[] = "QodeAssist.autoCompletionCharThreshold";
const char СС_AUTO_COMPLETION_TYPING_INTERVAL[] = "QodeAssist.autoCompletionTypingInterval";
const char MAX_FILE_THRESHOLD[] = "QodeAssist.maxFileThreshold";
const char CC_MULTILINE_COMPLETION[] = "QodeAssist.ccMultilineCompletion";
const char CC_MODEL_OUTPUT_HANDLER[] = "QodeAssist.ccModelOutputHandler";
const char CUSTOM_JSON_TEMPLATE[] = "QodeAssist.customJsonTemplate";
const char CA_AUTO_APPLY_FILE_EDITS[] = "QodeAssist.caAutoApplyFileEdits";
const char CA_TOKENS_THRESHOLD[] = "QodeAssist.caTokensThreshold";
const char CA_LINK_OPEN_FILES[] = "QodeAssist.caLinkOpenFiles";
const char CA_AUTOSAVE[] = "QodeAssist.caAutosave";
const char CC_CUSTOM_LANGUAGES[] = "QodeAssist.ccCustomLanguages";

const char CA_ENABLE_CHAT_IN_BOTTOM_TOOLBAR[] = "QodeAssist.caEnableChatInBottomToolbar";
const char CA_ENABLE_CHAT_IN_NAVIGATION_PANEL[] = "QodeAssist.caEnableChatInNavigationPanel";
const char CA_USE_TOOLS[] = "QodeAssist.caUseTools";
const char CA_ALLOW_FILE_SYSTEM_READ[] = "QodeAssist.caAllowFileSystemRead";
const char CA_ALLOW_FILE_SYSTEM_WRITE[] = "QodeAssist.caAllowFileSystemWrite";
const char CA_ALLOW_ACCESS_OUTSIDE_PROJECT[] = "QodeAssist.caAllowAccessOutsideProject";
const char CA_ENABLE_EDIT_FILE_TOOL[] = "QodeAssist.caEnableEditFileTool";

const char QODE_ASSIST_GENERAL_OPTIONS_ID[] = "QodeAssist.GeneralOptions";
const char QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID[] = "QodeAssist.1GeneralSettingsPageId";
const char QODE_ASSIST_CODE_COMPLETION_SETTINGS_PAGE_ID[]
    = "QodeAssist.2CodeCompletionSettingsPageId";
const char QODE_ASSIST_CHAT_ASSISTANT_SETTINGS_PAGE_ID[]
    = "QodeAssist.3ChatAssistantSettingsPageId";
const char QODE_ASSIST_TOOLS_SETTINGS_PAGE_ID[] = "QodeAssist.4ToolsSettingsPageId";
const char QODE_ASSIST_CUSTOM_PROMPT_SETTINGS_PAGE_ID[] = "QodeAssist.5CustomPromptSettingsPageId";

const char QODE_ASSIST_GENERAL_OPTIONS_CATEGORY[] = "QodeAssist.Category";
const char QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY[] = "QodeAssist";

// Provider Settings Page ID
const char QODE_ASSIST_PROVIDER_SETTINGS_PAGE_ID[] = "QodeAssist.6ProviderSettingsPageId";

// Provider API Keys
const char OPEN_ROUTER_API_KEY[] = "QodeAssist.openRouterApiKey";
const char OPEN_ROUTER_API_KEY_HISTORY[] = "QodeAssist.openRouterApiKeyHistory";
const char OPEN_AI_COMPAT_API_KEY[] = "QodeAssist.openAiCompatApiKey";
const char OPEN_AI_COMPAT_API_KEY_HISTORY[] = "QodeAssist.openAiCompatApiKeyHistory";
const char CLAUDE_API_KEY[] = "QodeAssist.claudeApiKey";
const char CLAUDE_API_KEY_HISTORY[] = "QodeAssist.claudeApiKeyHistory";
const char OPEN_AI_API_KEY[] = "QodeAssist.openAiApiKey";
const char OPEN_AI_API_KEY_HISTORY[] = "QodeAssist.openAiApiKeyHistory";
const char MISTRAL_AI_API_KEY[] = "QodeAssist.mistralAiApiKey";
const char MISTRAL_AI_API_KEY_HISTORY[] = "QodeAssist.mistralAiApiKeyHistory";
const char CODESTRAL_API_KEY[] = "QodeAssist.codestralApiKey";
const char CODESTRAL_API_KEY_HISTORY[] = "QodeAssist.codestralApiKeyHistory";
const char GOOGLE_AI_API_KEY[] = "QodeAssist.googleAiApiKey";
const char GOOGLE_AI_API_KEY_HISTORY[] = "QodeAssist.googleAiApiKeyHistory";
const char OLLAMA_BASIC_AUTH_API_KEY[] = "QodeAssist.ollamaBasicAuthApiKey";
const char OLLAMA_BASIC_AUTH_API_KEY_HISTORY[] = "QodeAssist.ollamaBasicAuthApiKeyHistory";

// context settings
const char CC_READ_FULL_FILE[] = "QodeAssist.ccReadFullFile";
const char CC_READ_STRINGS_BEFORE_CURSOR[] = "QodeAssist.ccReadStringsBeforeCursor";
const char CC_READ_STRINGS_AFTER_CURSOR[] = "QodeAssist.ccReadStringsAfterCursor";
const char CC_USE_SYSTEM_PROMPT[] = "QodeAssist.ccUseSystemPrompt";
const char CC_SYSTEM_PROMPT[] = "QodeAssist.ccSystemPrompt";
const char CC_SYSTEM_PROMPT_FOR_NON_FIM[] = "QodeAssist.ccSystemPromptForNonFim";
const char CC_USE_USER_TEMPLATE[] = "QodeAssist.ccUseUserTemplate";
const char CC_USER_TEMPLATE[] = "QodeAssist.ccUserTemplate";
const char CC_USE_PROJECT_CHANGES_CACHE[] = "QodeAssist.ccUseProjectChangesCache";
const char CC_MAX_CHANGES_CACHE_SIZE[] = "QodeAssist.ccMaxChangesCacheSize";
const char CA_USE_SYSTEM_PROMPT[] = "QodeAssist.useChatSystemPrompt";
const char CA_SYSTEM_PROMPT[] = "QodeAssist.chatSystemPrompt";

// quick refactor command settings
const char CC_QUICK_REFACTOR_SYSTEM_PROMPT[] = "QodeAssist.ccQuickRefactorSystemPrompt";
const char CC_USE_OPEN_FILES_IN_QUICK_REFACTOR[] = "QodeAssist.ccUseOpenFilesInQuickRefactor";

// preset prompt settings
const char CC_TEMPERATURE[] = "QodeAssist.ccTemperature";
const char CC_MAX_TOKENS[] = "QodeAssist.ccMaxTokens";
const char CC_USE_TOP_P[] = "QodeAssist.ccUseTopP";
const char CC_TOP_P[] = "QodeAssist.ccTopP";
const char CC_USE_TOP_K[] = "QodeAssist.ccUseTopK";
const char CC_TOP_K[] = "QodeAssist.ccTopK";
const char CC_USE_PRESENCE_PENALTY[] = "QodeAssist.ccUsePresencePenalty";
const char CC_PRESENCE_PENALTY[] = "QodeAssist.ccPresencePenalty";
const char CC_USE_FREQUENCY_PENALTY[] = "QodeAssist.fimUseFrequencyPenalty";
const char CC_FREQUENCY_PENALTY[] = "QodeAssist.fimFrequencyPenalty";
const char CC_OLLAMA_LIVETIME[] = "QodeAssist.fimOllamaLivetime";
const char CC_OLLAMA_CONTEXT_WINDOW[] = "QodeAssist.ccOllamaContextWindow";
const char CA_TEMPERATURE[] = "QodeAssist.chatTemperature";
const char CA_MAX_TOKENS[] = "QodeAssist.chatMaxTokens";
const char CA_USE_TOP_P[] = "QodeAssist.chatUseTopP";
const char CA_TOP_P[] = "QodeAssist.chatTopP";
const char CA_USE_TOP_K[] = "QodeAssist.chatUseTopK";
const char CA_TOP_K[] = "QodeAssist.chatTopK";
const char CA_USE_PRESENCE_PENALTY[] = "QodeAssist.chatUsePresencePenalty";
const char CA_PRESENCE_PENALTY[] = "QodeAssist.chatPresencePenalty";
const char CA_USE_FREQUENCY_PENALTY[] = "QodeAssist.chatUseFrequencyPenalty";
const char CA_FREQUENCY_PENALTY[] = "QodeAssist.chatFrequencyPenalty";
const char CA_OLLAMA_LIVETIME[] = "QodeAssist.chatOllamaLivetime";
const char CA_OLLAMA_CONTEXT_WINDOW[] = "QodeAssist.caOllamaContextWindow";
const char CA_TEXT_FONT_FAMILY[] = "QodeAssist.caTextFontFamily";
const char CA_TEXT_FONT_SIZE[] = "QodeAssist.caTextFontSize";
const char CA_CODE_FONT_FAMILY[] = "QodeAssist.caCodeFontFamily";
const char CA_CODE_FONT_SIZE[] = "QodeAssist.caCodeFontSize";
const char CA_TEXT_FORMAT[] = "QodeAssist.caTextFormat";
const char CA_CHAT_RENDERER[] = "QodeAssist.caChatRenderer";

} // namespace QodeAssist::Constants
