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

namespace QodeAssist {

LLMProvidersManager &LLMProvidersManager::instance()
{
    static LLMProvidersManager instance;
    return instance;
}

QStringList LLMProvidersManager::getProviderNames() const
{
    return m_providers.keys();
}

void LLMProvidersManager::setCurrentProvider(const QString &name)
{
    if (m_providers.contains(name)) {
        m_currentProviderName = name;
    }
}

Providers::LLMProvider *LLMProvidersManager::getCurrentProvider()
{
    if (m_currentProviderName.isEmpty()) {
        return nullptr;
    }

    return m_providers[m_currentProviderName];
}

LLMProvidersManager::~LLMProvidersManager()
{
    qDeleteAll(m_providers);
}

} // namespace QodeAssist
