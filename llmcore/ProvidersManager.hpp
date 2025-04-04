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

#pragma once

#include <QString>

#include "IProviderRegistry.hpp"
#include <QMap>

namespace QodeAssist::LLMCore {

class ProvidersManager : public IProviderRegistry
{
public:
    static ProvidersManager &instance();
    ~ProvidersManager();

    template<typename T>
    void registerProvider()
    {
        static_assert(std::is_base_of<Provider, T>::value, "T must inherit from Provider");
        T *provider = new T();
        QString name = provider->name();
        m_providers[name] = provider;
    }

    Provider *getProviderByName(const QString &providerName) override;

    QStringList providersNames() const override;

private:
    ProvidersManager() = default;
    ProvidersManager(const ProvidersManager &) = delete;
    ProvidersManager &operator=(const ProvidersManager &) = delete;

    QMap<QString, Provider *> m_providers;
};

} // namespace QodeAssist::LLMCore
