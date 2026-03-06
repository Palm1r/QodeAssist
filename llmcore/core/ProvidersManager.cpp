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

#include "ProvidersManager.hpp"

namespace QodeAssist::LLMCore {

ProvidersManager &ProvidersManager::instance()
{
    static ProvidersManager instance;
    return instance;
}

QStringList ProvidersManager::providersNames() const
{
    return m_providers.keys();
}

ProvidersManager::~ProvidersManager()
{
    qDeleteAll(m_providers);
}

Provider *ProvidersManager::getProviderByName(const QString &providerName)
{
    if (!m_providers.contains(providerName))
        return m_providers.first();
    return m_providers[providerName];
}

} // namespace QodeAssist::LLMCore
