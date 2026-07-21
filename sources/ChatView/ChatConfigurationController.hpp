// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>

#include <QHash>
#include <QObject>
#include <QStringList>

#include "acp/AgentDefinition.hpp"

namespace QodeAssist::Acp {
class AgentCatalogStore;
}

namespace QodeAssist::Chat {

class ChatConfigurationController : public QObject
{
    Q_OBJECT

public:
    explicit ChatConfigurationController(QObject *parent = nullptr);

    QStringList availableConfigurations() const;
    QString currentConfiguration() const;

    void setAgentCatalog(Acp::AgentCatalogStore *store);

    void loadAvailableConfigurations();
    void applyConfiguration(const QString &configName);

    void setBoundAgent(const Acp::AgentDefinition &agent);
    void clearBoundAgent();

    std::optional<Acp::AgentDefinition> agentById(const QString &agentId);

signals:
    void availableConfigurationsChanged();
    void currentConfigurationChanged();
    void agentRequested(const QodeAssist::Acp::AgentDefinition &agent);
    void llmRequested();

private:
    void updateCurrentConfiguration();

    Acp::AgentCatalogStore *m_agents = nullptr;
    QStringList m_availableConfigurations;
    QHash<QString, QString> m_agentIdByEntry;
    QString m_currentConfiguration;
    QString m_boundAgentName;
};

} // namespace QodeAssist::Chat
