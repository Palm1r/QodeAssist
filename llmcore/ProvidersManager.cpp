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

#include "ProvidersManager.hpp"
#include "Logger.hpp"
#include <coreplugin/messagemanager.h>

namespace QodeAssist::LLMCore {

ProvidersManager &ProvidersManager::instance()
{
    static ProvidersManager instance;
    return instance;
}

Provider *ProvidersManager::setCurrentFimProvider(const QString &name)
{
    LOG_MESSAGE("Setting current FIM provider to: " + name);
    if (!m_providers.contains(name)) {
        LOG_MESSAGE("Can't find provider with name: " + name);
        return nullptr;
    }

    m_currentFimProvider = m_providers[name];
    return m_currentFimProvider;
}

Provider *ProvidersManager::setCurrentChatProvider(const QString &name)
{
    LOG_MESSAGE("Setting current chat provider to: " + name);
    if (!m_providers.contains(name)) {
        LOG_MESSAGE("Can't find chat provider with name: " + name);
        return nullptr;
    }

    m_currentChatProvider = m_providers[name];
    return m_currentChatProvider;
}

Provider *ProvidersManager::getCurrentFimProvider()
{
    if (m_currentFimProvider == nullptr) {
        LOG_MESSAGE("Current fim provider is null, return first");
        return m_providers.first();
    }

    return m_currentFimProvider;
}

Provider *ProvidersManager::getCurrentChatProvider()
{
    if (m_currentChatProvider == nullptr) {
        LOG_MESSAGE("Current chat provider is null, return first");
        return m_providers.first();
    }

    return m_currentChatProvider;
}

QStringList ProvidersManager::providersNames() const
{
    return m_providers.keys();
}

ProvidersManager::~ProvidersManager()
{
    qDeleteAll(m_providers);
}

} // namespace QodeAssist::LLMCore
