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
const char CC_MODEL_HISTORY[] = "QodeAssist.ccModelHistory";
const char CC_TEMPLATE[] = "QodeAssist.ccTemplate";
const char CC_URL[] = "QodeAssist.ccUrl";
const char CC_URL_HISTORY[] = "QodeAssist.ccUrlHistory";

const char CA_PROVIDER[] = "QodeAssist.caProvider";
const char CA_MODEL[] = "QodeAssist.caModel";
const char CA_MODEL_HISTORY[] = "QodeAssist.caModelHistory";
const char CA_TEMPLATE[] = "QodeAssist.caTemplate";
const char CA_URL[] = "QodeAssist.caUrl";
const char CA_URL_HISTORY[] = "QodeAssist.caUrlHistory";

// settings
const char ENABLE_QODE_ASSIST[] = "QodeAssist.enableQodeAssist";
const char CC_AUTO_COMPLETION[] = "QodeAssist.ccAutoCompletion";
const char ENABLE_LOGGING[] = "QodeAssist.enableLogging";
const char PROVIDER_PATHS[] = "QodeAssist.providerPaths";
const char СС_START_SUGGESTION_TIMER[] = "QodeAssist.startSuggestionTimer";
const char СС_AUTO_COMPLETION_CHAR_THRESHOLD[] = "QodeAssist.autoCompletionCharThreshold";
const char СС_AUTO_COMPLETION_TYPING_INTERVAL[] = "QodeAssist.autoCompletionTypingInterval";
const char MAX_FILE_THRESHOLD[] = "QodeAssist.maxFileThreshold";
const char CC_MULTILINE_COMPLETION[] = "QodeAssist.ccMultilineCompletion";
const char CUSTOM_JSON_TEMPLATE[] = "QodeAssist.customJsonTemplate";
const char CA_TOKENS_THRESHOLD[] = "QodeAssist.caTokensThreshold";

const char QODE_ASSIST_GENERAL_OPTIONS_ID[] = "QodeAssist.GeneralOptions";
const char QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID[] = "QodeAssist.1GeneralSettingsPageId";
const char QODE_ASSIST_CODE_COMPLETION_SETTINGS_PAGE_ID[]
    = "QodeAssist.2CodeCompletionSettingsPageId";
const char QODE_ASSIST_CHAT_ASSISTANT_SETTINGS_PAGE_ID[]
    = "QodeAssist.3ChatAssistantSettingsPageId";
const char QODE_ASSIST_CUSTOM_PROMPT_SETTINGS_PAGE_ID[] = "QodeAssist.4CustomPromptSettingsPageId";

const char QODE_ASSIST_GENERAL_OPTIONS_CATEGORY[] = "QodeAssist.Category";
const char QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY[] = "Qode Assist";

const char QODE_ASSIST_REQUEST_SUGGESTION[] = "QodeAssist.RequestSuggestion";

// context settings
const char CC_READ_FULL_FILE[] = "QodeAssist.ccReadFullFile";
const char CC_READ_STRINGS_BEFORE_CURSOR[] = "QodeAssist.ccReadStringsBeforeCursor";
const char CC_READ_STRINGS_AFTER_CURSOR[] = "QodeAssist.ccReadStringsAfterCursor";
const char CC_USE_SYSTEM_PROMPT[] = "QodeAssist.ccUseSystemPrompt";
const char CC_USE_FILE_PATH_IN_CONTEXT[] = "QodeAssist.ccUseFilePathInContext";
const char CC_SYSTEM_PROMPT[] = "QodeAssist.ccSystemPrompt";
const char CC_USE_PROJECT_CHANGES_CACHE[] = "QodeAssist.ccUseProjectChangesCache";
const char CC_MAX_CHANGES_CACHE_SIZE[] = "QodeAssist.ccMaxChangesCacheSize";
const char CA_USE_SYSTEM_PROMPT[] = "QodeAssist.useChatSystemPrompt";
const char CA_SYSTEM_PROMPT[] = "QodeAssist.chatSystemPrompt";

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
const char CC_API_KEY[] = "QodeAssist.apiKey";
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
const char CA_API_KEY[] = "QodeAssist.chatApiKey";

} // namespace QodeAssist::Constants
