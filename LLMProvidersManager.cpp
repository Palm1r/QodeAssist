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

#include "LLMProvidersManager.hpp"

#include "QodeAssistUtils.hpp"

namespace QodeAssist {

LLMProvidersManager &LLMProvidersManager::instance()
{
    static LLMProvidersManager instance;
    return instance;
}

Providers::LLMProvider *LLMProvidersManager::setCurrentFimProvider(const QString &name)
{
    logMessage("Setting current FIM provider to: " + name);
    if (!m_providers.contains(name)) {
        logMessage("Can't find provider with name: " + name);
        return nullptr;
    }

    m_currentFimProvider = m_providers[name];
    return m_currentFimProvider;
}

Providers::LLMProvider *LLMProvidersManager::setCurrentChatProvider(const QString &name)
{
    logMessage("Setting current chat provider to: " + name);
    if (!m_providers.contains(name)) {
        logMessage("Can't find chat provider with name: " + name);
        return nullptr;
    }

    m_currentChatProvider = m_providers[name];
    return m_currentChatProvider;
}

Providers::LLMProvider *LLMProvidersManager::getCurrentFimProvider()
{
    if (m_currentFimProvider == nullptr) {
        logMessage("Current fim provider is null");
        return nullptr;
    }

    return m_currentFimProvider;
}

Providers::LLMProvider *LLMProvidersManager::getCurrentChatProvider()
{
    if (m_currentChatProvider == nullptr) {
        logMessage("Current chat provider is null");
        return nullptr;
    }

    return m_currentChatProvider;
}

QStringList LLMProvidersManager::providersNames() const
{
    return m_providers.keys();
}

LLMProvidersManager::~LLMProvidersManager()
{
    qDeleteAll(m_providers);
}

} // namespace QodeAssist
