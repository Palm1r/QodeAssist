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
inline const char *RESET_TO_DEFAULTS = QT_TRANSLATE_NOOP("QtC::QodeAssist",
                                                         "Reset Page to Defaults");
inline const char *SELECT = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Select...");
inline const char *PROVIDER = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Provider:");
inline const char *MODEL = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Model:");
inline const char *TEMPLATE = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Template:");
inline const char *URL = QT_TRANSLATE_NOOP("QtC::QodeAssist", "URL:");
inline const char *STATUS = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Status:");
inline const char *TEST = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Test");
inline const char *ENABLE_LOG = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Enable Logging");
inline const char *CODE_COMPLETION = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Code Completion");
inline const char *CHAT_ASSISTANT = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Chat Assistant");
inline const char *RESET_SETTINGS = QT_TRANSLATE_NOOP("QtC::QodeAssist", "Reset Settings");
inline const char *CONFIRMATION
    = QT_TRANSLATE_NOOP("QtC::QodeAssist",
                        "Are you sure you want to reset all settings to default values?");
} // namespace TrConstants

struct Tr
{
    Q_DECLARE_TR_FUNCTIONS(QtC::QodeAssist)
};

} // namespace QodeAssist::Settings
