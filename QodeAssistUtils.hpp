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

#include <QString>
#include <coreplugin/messagemanager.h>
#include <utils/qtcassert.h>

namespace QodeAssist {

inline bool &loggingEnabled()
{
    static bool enabled = false;
    return enabled;
}

inline void setLoggingEnabled(bool enable)
{
    loggingEnabled() = enable;
}

inline void logMessage(const QString &message, bool silent = true)
{
    if (!loggingEnabled())
        return;

    const QString prefixedMessage = QLatin1String("[Qode Assist] ") + message;
    if (silent) {
        Core::MessageManager::writeSilently(prefixedMessage);
    } else {
        Core::MessageManager::writeFlashing(prefixedMessage);
    }
}

inline void logMessages(const QStringList &messages, bool silent = true)
{
    if (!loggingEnabled())
        return;

    QStringList prefixedMessages;
    for (const QString &message : messages) {
        prefixedMessages << (QLatin1String("[Qode Assist] ") + message);
    }
    if (silent) {
        Core::MessageManager::writeSilently(prefixedMessages);
    } else {
        Core::MessageManager::writeFlashing(prefixedMessages);
    }
}

} // namespace QodeAssist
