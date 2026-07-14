// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

#include "providers/IProviderRegistry.hpp"
#include <QMap>

namespace QodeAssist::Providers {

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

} // namespace QodeAssist::Providers
