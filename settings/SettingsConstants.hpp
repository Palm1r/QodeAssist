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

namespace QodeAssist::Constants {

const char ACTION_ID[] = "QodeAssist.Action";
const char MENU_ID[] = "QodeAssist.Menu";

// new settings
const char CC_PROVIDER[] = "QodeAssist.ccProvider";
const char CC_MODEL[] = "QodeAssist.ccModel";
const char CC_TEMPLATE[] = "QodeAssist.ccTemplate";
const char CC_URL[] = "QodeAssist.ccUrl";

const char CA_PROVIDER[] = "QodeAssist.caProvider";
const char CA_MODEL[] = "QodeAssist.caModel";
const char CA_TEMPLATE[] = "QodeAssist.caTemplate";
const char CA_URL[] = "QodeAssist.caUrl";

// settings
const char ENABLE_QODE_ASSIST[] = "QodeAssist.enableQodeAssist";
const char ENABLE_AUTO_COMPLETE[] = "QodeAssist.enableAutoComplete";
const char ENABLE_LOGGING[] = "QodeAssist.enableLogging";
const char LLM_PROVIDERS[] = "QodeAssist.llmProviders";
const char URL[] = "QodeAssist.url";
const char END_POINT[] = "QodeAssist.endPoint";
const char MODEL_NAME[] = "QodeAssist.modelName";
const char SELECT_MODELS[] = "QodeAssist.selectModels";
const char FIM_PROMPTS[] = "QodeAssist.fimPrompts";
const char PROVIDER_PATHS[] = "QodeAssist.providerPaths";
const char START_SUGGESTION_TIMER[] = "QodeAssist.startSuggestionTimer";
const char AUTO_COMPLETION_CHAR_THRESHOLD[] = "QodeAssist.autoCompletionCharThreshold";
const char AUTO_COMPLETION_TYPING_INTERVAL[] = "QodeAssist.autoCompletionTypingInterval";
const char MAX_FILE_THRESHOLD[] = "QodeAssist.maxFileThreshold";
const char MULTILINE_COMPLETION[] = "QodeAssist.multilineCompletion";
const char CUSTOM_JSON_TEMPLATE[] = "QodeAssist.customJsonTemplate";
const char CHAT_LLM_PROVIDERS[] = "QodeAssist.chatLlmProviders";
const char CHAT_URL[] = "QodeAssist.chatUrl";
const char CHAT_END_POINT[] = "QodeAssist.chatEndPoint";
const char CHAT_MODEL_NAME[] = "QodeAssist.chatModelName";
const char CHAT_SELECT_MODELS[] = "QodeAssist.chatSelectModels";
const char CHAT_PROMPTS[] = "QodeAssist.chatPrompts";
const char CHAT_TOKENS_THRESHOLD[] = "QodeAssist.chatTokensThreshold";

const char QODE_ASSIST_GENERAL_OPTIONS_ID[] = "QodeAssist.GeneralOptions";
const char QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID[] = "QodeAssist.1GeneralSettingsPageId";
const char QODE_ASSIST_CONTEXT_SETTINGS_PAGE_ID[] = "QodeAssist.2ContextSettingsPageId";
const char QODE_ASSIST_PRESET_PROMPTS_SETTINGS_PAGE_ID[]
    = "QodeAssist.3PresetPromptsSettingsPageId";
const char QODE_ASSIST_CUSTOM_PROMPT_SETTINGS_PAGE_ID[] = "QodeAssist.4CustomPromptSettingsPageId";

const char QODE_ASSIST_GENERAL_OPTIONS_CATEGORY[] = "QodeAssist.Category";
const char QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY[] = "Qode Assist";

const char QODE_ASSIST_REQUEST_SUGGESTION[] = "QodeAssist.RequestSuggestion";

// context settings
const char READ_FULL_FILE[] = "QodeAssist.readFullFile";
const char READ_STRINGS_BEFORE_CURSOR[] = "QodeAssist.readStringsBeforeCursor";
const char READ_STRINGS_AFTER_CURSOR[] = "QodeAssist.readStringsAfterCursor";
const char USE_SYSTEM_PROMPT[] = "QodeAssist.useSystemPrompt";
const char USE_FILE_PATH_IN_CONTEXT[] = "QodeAssist.useFilePathInContext";
const char SYSTEM_PROMPT[] = "QodeAssist.systemPrompt";
const char USE_PROJECT_CHANGES_CACHE[] = "QodeAssist.useProjectChangesCache";
const char MAX_CHANGES_CACHE_SIZE[] = "QodeAssist.maxChangesCacheSize";
const char USE_CHAT_SYSTEM_PROMPT[] = "QodeAssist.useChatSystemPrompt";
const char CHAT_SYSTEM_PROMPT[] = "QodeAssist.chatSystemPrompt";

// preset prompt settings
const char FIM_TEMPERATURE[] = "QodeAssist.fimTemperature";
const char FIM_MAX_TOKENS[] = "QodeAssist.fimMaxTokens";
const char FIM_USE_TOP_P[] = "QodeAssist.fimUseTopP";
const char FIM_TOP_P[] = "QodeAssist.fimTopP";
const char FIM_USE_TOP_K[] = "QodeAssist.fimUseTopK";
const char FIM_TOP_K[] = "QodeAssist.fimTopK";
const char FIM_USE_PRESENCE_PENALTY[] = "QodeAssist.fimUsePresencePenalty";
const char FIM_PRESENCE_PENALTY[] = "QodeAssist.fimPresencePenalty";
const char FIM_USE_FREQUENCY_PENALTY[] = "QodeAssist.fimUseFrequencyPenalty";
const char FIM_FREQUENCY_PENALTY[] = "QodeAssist.fimFrequencyPenalty";
const char FIM_OLLAMA_LIVETIME[] = "QodeAssist.fimOllamaLivetime";
const char FIM_API_KEY[] = "QodeAssist.apiKey";
const char CHAT_TEMPERATURE[] = "QodeAssist.chatTemperature";
const char CHAT_MAX_TOKENS[] = "QodeAssist.chatMaxTokens";
const char CHAT_USE_TOP_P[] = "QodeAssist.chatUseTopP";
const char CHAT_TOP_P[] = "QodeAssist.chatTopP";
const char CHAT_USE_TOP_K[] = "QodeAssist.chatUseTopK";
const char CHAT_TOP_K[] = "QodeAssist.chatTopK";
const char CHAT_USE_PRESENCE_PENALTY[] = "QodeAssist.chatUsePresencePenalty";
const char CHAT_PRESENCE_PENALTY[] = "QodeAssist.chatPresencePenalty";
const char CHAT_USE_FREQUENCY_PENALTY[] = "QodeAssist.chatUseFrequencyPenalty";
const char CHAT_FREQUENCY_PENALTY[] = "QodeAssist.chatFrequencyPenalty";
const char CHAT_OLLAMA_LIVETIME[] = "QodeAssist.chatOllamaLivetime";
const char CHAT_API_KEY[] = "QodeAssist.chatApiKey";

} // namespace QodeAssist::Constants
