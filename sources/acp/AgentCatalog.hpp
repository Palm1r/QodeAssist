// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <array>
#include <optional>

#include <QList>
#include <QString>

#include "acp/AgentDefinition.hpp"

namespace QodeAssist::Acp {

class AgentCatalog
{
public:
    void setLayer(AgentSource source, const QList<AgentDefinition> &agents);

    QList<AgentDefinition> agents() const { return m_merged; }
    QList<AgentDefinition> launchableAgents() const;
    std::optional<AgentDefinition> agent(const QString &id) const;

    int size() const { return m_merged.size(); }

private:
    void rebuild();

    std::array<QList<AgentDefinition>, agentSourceCount> m_layers;
    QList<AgentDefinition> m_merged;
};

} // namespace QodeAssist::Acp
