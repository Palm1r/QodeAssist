// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentCatalog.hpp"

#include <algorithm>

#include <QHash>

namespace QodeAssist::Acp {

namespace {

constexpr std::array<AgentSource, agentSourceCount> mergeOrderLowestPriorityFirst{
    AgentSource::BundledSnapshot, AgentSource::LiveRegistry, AgentSource::UserFile};

} // namespace

void AgentCatalog::setLayer(AgentSource source, const QList<AgentDefinition> &agents)
{
    m_layers[static_cast<int>(source)] = agents;
    rebuild();
}

QList<AgentDefinition> AgentCatalog::launchableAgents() const
{
    QList<AgentDefinition> result;
    for (const AgentDefinition &agent : m_merged) {
        if (agent.isLaunchable())
            result.append(agent);
    }
    return result;
}

std::optional<AgentDefinition> AgentCatalog::agent(const QString &id) const
{
    for (const AgentDefinition &agent : m_merged) {
        if (agent.id == id)
            return agent;
    }
    return std::nullopt;
}

void AgentCatalog::rebuild()
{
    QHash<QString, AgentDefinition> byId;
    for (AgentSource source : mergeOrderLowestPriorityFirst) {
        for (const AgentDefinition &agent : std::as_const(m_layers[static_cast<int>(source)]))
            byId.insert(agent.id, agent);
    }

    m_merged = byId.values();
    std::sort(
        m_merged.begin(),
        m_merged.end(),
        [](const AgentDefinition &left, const AgentDefinition &right) {
            const int byName = QString::compare(left.name, right.name, Qt::CaseInsensitive);
            if (byName != 0)
                return byName < 0;
            return left.id < right.id;
        });
}

} // namespace QodeAssist::Acp
