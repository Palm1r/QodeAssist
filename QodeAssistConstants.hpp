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

// settings
const char ENABLE_QODE_ASSIST[] = "QodeAssist.enableQodeAssist";
const char ENABLE_AUTO_COMPLETE[] = "QodeAssist.enableAutoComplete";
const char ENABLE_LOGGING[] = "QodeAssist.enableLogging";
const char LLM_PROVIDERS[] = "QodeAssist.llmProviders";
const char URL[] = "QodeAssist.url";
const char PORT[] = "QodeAssist.port";
const char END_POINT[] = "QodeAssist.endPoint";
const char MODEL_NAME[] = "QodeAssist.modelName";
const char SELECT_MODELS[] = "QodeAssist.selectModels";
const char FIM_PROMPTS[] = "QodeAssist.fimPrompts";
const char TEMPERATURE[] = "QodeAssist.temperature";
const char MAX_TOKENS[] = "QodeAssist.maxTokens";
const char READ_FULL_FILE[] = "QodeAssist.readFullFile";
const char READ_STRINGS_BEFORE_CURSOR[] = "QodeAssist.readStringsBeforeCursor";
const char READ_STRINGS_AFTER_CURSOR[] = "QodeAssist.readStringsAfterCursor";
const char USE_TOP_P[] = "QodeAssist.useTopP";
const char TOP_P[] = "QodeAssist.topP";
const char USE_TOP_K[] = "QodeAssist.useTopK";
const char TOP_K[] = "QodeAssist.topK";
const char USE_PRESENCE_PENALTY[] = "QodeAssist.usePresencePenalty";
const char PRESENCE_PENALTY[] = "QodeAssist.presencePenalty";
const char USE_FREQUENCY_PENALTY[] = "QodeAssist.useFrequencyPenalty";
const char FREQUENCY_PENALTY[] = "QodeAssist.frequencyPenalty";
const char PROVIDER_PATHS[] = "QodeAssist.providerPaths";
const char START_SUGGESTION_TIMER[] = "QodeAssist.startSuggestionTimer";
const char MAX_FILE_THRESHOLD[] = "QodeAssist.maxFileThreshold";
const char OLLAMA_LIVETIME[] = "QodeAssist.ollamaLivetime";

const char QODE_ASSIST_GENERAL_OPTIONS_ID[] = "QodeAssist.GeneralOptions";
const char QODE_ASSIST_GENERAL_OPTIONS_CATEGORY[] = "QodeAssist.Category";
const char QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY[] = "Qode Assist";

const char QODE_ASSIST_REQUEST_SUGGESTION[] = "QodeAssist.RequestSuggestion";

} // namespace QodeAssist::Constants
