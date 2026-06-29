// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>
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
} // namespace Providers

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
    [[nodiscard]] static QString userConfigDir();

    [[nodiscard]] const AgentConfig *configByName(const QString &name) const;
    [[nodiscard]] QStringList configNames() const;
    [[nodiscard]] const std::vector<AgentConfig> &configs() const noexcept { return m_configs; }

    Agent *create(const QString &name, QObject *parent, QString *errorOut = nullptr) const;

    Agent *createFromFile(
        const QString &tomlPath, QObject *parent, QString *errorOut = nullptr) const;

    void registerConfig(AgentConfig config);
    void clear();

    bool setAgentModelOverride(const QString &name, const QString &model, QString *error = nullptr);
    [[nodiscard]] QString agentModelOverride(const QString &name) const;
    void clearAgentModelOverride(const QString &name);

    bool setAgentProviderOverride(
        const QString &name, const QString &providerInstance, QString *error = nullptr);
    [[nodiscard]] QString agentProviderOverride(const QString &name) const;
    void clearAgentProviderOverride(const QString &name);

    bool setAgentToolsOverride(
        const QString &name, std::optional<bool> enabled, QString *error = nullptr);
    [[nodiscard]] std::optional<bool> agentToolsOverride(const QString &name) const;
    void clearAgentToolsOverride(const QString &name);

    [[nodiscard]] Providers::ProviderInstanceFactory *instanceFactory() const noexcept;

signals:
    void agentsChanged();
    void agentModelChanged(const QString &name);
    void agentProviderChanged(const QString &name);
    void agentToolsChanged(const QString &name);

private:
    std::vector<AgentConfig> m_configs;
    QHash<QString, qsizetype> m_indexByName;
    QHash<QString, QString> m_baseModelByName;
    QHash<QString, QString> m_baseProviderByName;
    QHash<QString, bool> m_baseToolsByName;
    QPointer<Providers::ProviderInstanceFactory> m_instanceFactory;
    QPointer<Providers::ProviderSecretsStore> m_secrets;
};

} // namespace QodeAssist
