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

#include "providers/LLMProvider.hpp"

namespace QodeAssist {

class LLMProvidersManager
{
public:
    static LLMProvidersManager &instance();
    ~LLMProvidersManager();

    template<typename T>
    void registerProvider()
    {
        static_assert(std::is_base_of<Providers::LLMProvider, T>::value,
                      "T must inherit from LLMProvider");
        T *provider = new T();
        QString name = provider->name();
        m_providers[name] = provider;
    }

    Providers::LLMProvider *setCurrentFimProvider(const QString &name);
    Providers::LLMProvider *setCurrentChatProvider(const QString &name);

    Providers::LLMProvider *getCurrentFimProvider();
    Providers::LLMProvider *getCurrentChatProvider();

    QStringList providersNames() const;

private:
    LLMProvidersManager() = default;
    LLMProvidersManager(const LLMProvidersManager &) = delete;
    LLMProvidersManager &operator=(const LLMProvidersManager &) = delete;

    QMap<QString, Providers::LLMProvider *> m_providers;
    Providers::LLMProvider *m_currentFimProvider = nullptr;
    Providers::LLMProvider *m_currentChatProvider = nullptr;
};

} // namespace QodeAssist
