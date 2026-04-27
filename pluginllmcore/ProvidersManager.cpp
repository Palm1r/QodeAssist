// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProvidersManager.hpp"

namespace QodeAssist::PluginLLMCore {

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

} // namespace QodeAssist::PluginLLMCore
