// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "AgentConfig.hpp"

namespace QodeAssist {

class Agent;

namespace Providers {
class ProviderInstanceFactory;
class ProviderSecretsStore;
}

class AgentFactory : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AgentFactory)
public:
    explicit AgentFactory(
        Providers::ProviderInstanceFactory *instanceFactory = nullptr,
        Providers::ProviderSecretsStore *secrets = nullptr,
        QObject *parent = nullptr);
    ~AgentFactory() override;

    void reload();

    [[nodiscard]] static QString userAgentsDir();

    [[nodiscard]] const AgentConfig *configByName(const QString &name) const;
    [[nodiscard]] QStringList configNames() const;
    [[nodiscard]] const std::vector<AgentConfig> &configs() const noexcept { return m_configs; }

    Agent *create(const QString &name, QObject *parent, QString *errorOut = nullptr) const;

    Agent *createFromFile(
        const QString &tomlPath, QObject *parent, QString *errorOut = nullptr) const;

    [[nodiscard]] QStringList lastLoadErrors() const { return m_errors; }
    [[nodiscard]] QStringList lastLoadWarnings() const { return m_warnings; }

    void registerConfig(AgentConfig config);
    void clear();

    [[nodiscard]] Providers::ProviderInstanceFactory *instanceFactory() const noexcept;
    [[nodiscard]] Providers::ProviderSecretsStore *secretsStore() const noexcept;

private:
    std::vector<AgentConfig> m_configs;
    QHash<QString, qsizetype> m_indexByName;
    QStringList m_errors;
    QStringList m_warnings;
    QPointer<Providers::ProviderInstanceFactory> m_instanceFactory;
    QPointer<Providers::ProviderSecretsStore> m_secrets;
};

} // namespace QodeAssist
