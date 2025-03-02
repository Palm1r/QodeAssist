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

#include <QCoreApplication>

namespace QodeAssist::Settings {

namespace TrConstants {
inline const char *ENABLE_QODE_ASSIST = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Enable Qode Assist");
inline const char *GENERAL = QT_TRANSLATE_NOOP("QtC::QodeAssist", "General");
inline const char *RESET_TO_DEFAULTS
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Reset Page to Defaults");
inline const char *CHECK_UPDATE = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Check Update");
inline const char *SELECT = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Select...");
inline const char *PROVIDER = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Provider:");
inline const char *MODEL = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Model:");
inline const char *TEMPLATE = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Template:");
inline const char *URL = QT_TRANSLATE_NOOP("QtC::QodeAssist", "URL:");
inline const char *STATUS = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Status:");
inline const char *TEST = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Test");
inline const char *ENABLE_LOG = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Enable Logging");
inline const char *ENABLE_CHECK_UPDATE_ON_START
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Check for updates when Qt Creator starts");
inline const char *ENABLE_CHAT = QT_TRANSLATE_NOOP(
    "QtC::QodeAssist",
    "Enable Chat(If you have performance issues try disabling this, need restart QtC)");

inline const char *CODE_COMPLETION = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Code Completion");
inline const char *CHAT_ASSISTANT = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Chat Assistant");
inline const char *RESET_SETTINGS = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Reset Settings");
inline const char *CONFIRMATION = QT_TRANSLATE_NOOP(
    "QtC::QodeAssist", "Are you sure you want to reset all settings to default values?");
inline const char *CURRENT_TEMPLATE_DESCRIPTION
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Current template description:");

inline const char CONNECTION_ERROR[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Connection Error");
inline const char NO_MODELS_FOUND[]
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Unable to retrieve the list of models from the server.");
inline const char CHECK_CONNECTION[] = QT_TRANSLATE_NOOP(
    "QtC::QodeAssist",
    "Please verify the following:\n"
    "- Server is running and accessible\n"
    "- URL is correct\n"
    "- Provider is properly configured\n"
    "- API key is correctly set (if required)\n\n"
    "You can try selecting a different provider or changing the URL:");
inline const char SELECT_PROVIDER[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Select Provider");
inline const char SELECT_URL[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Select URL");
inline const char CLOSE[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Close");
inline const char MODEL_SELECTION[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Model Selection");
inline const char MODEL_LISTING_NOT_SUPPORTED_INFO[] = QT_TRANSLATE_NOOP(
    "QtC::QodeAssist",
    "Select from previously used models or enter a new model name.\n\n"
    "If entering a new model name:\n"
    "• For providers with automatic listing - ensure the model is installed\n"
    "• For providers without listing support - check provider's documentation\n"
    "• Make sure the model name matches exactly");
inline const char MODEL_NAME[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Model name:");
inline const char OK[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "OK");
inline const char CANCEL[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Cancel");
inline const char ENTER_MODEL_MANUALLY[]
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Enter Model Manually");
inline const char CONFIGURE_API_KEY[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Configure API Key");
inline const char URL_SELECTION[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "URL Selection");
inline const char URL_SELECTION_INFO[] = QT_TRANSLATE_NOOP(
    "QtC::QodeAssist",
    "Select from the list of default and previously used URLs, or enter a custom one.\n"
    "Please ensure the selected URL is accessible and the service is running.");
inline const char PREDEFINED_URL[]
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Use default provider URL or from history");
inline const char CUSTOM_URL[] = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Enter custom URL");
inline const char ENTER_MODEL_MANUALLY_BUTTON[]
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Enter Model Name Manually");
inline const char AUTO_COMPLETION_SETTINGS[]
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Auto Completion Settings");

inline const char ADD_NEW_PRESET[]
    = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Add new preset for language");

} // namespace TrConstants

struct Tr
{
    Q_DECLARE_TR_FUNCTIONS(QtC::QodeAssist)
};

} // namespace QodeAssist::Settings
