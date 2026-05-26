// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include <QObject>
#include <QString>

namespace Core { class IOptionsPage; }

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

class AgentsPageNavigator : public QObject
{
    Q_OBJECT
public:
    explicit AgentsPageNavigator(QObject *parent = nullptr);

    void requestSelectAgent(const QString &name);
    QString takePendingSelection();

signals:
    void selectAgentRequested(const QString &name);

private:
    QString m_pending;
};

std::unique_ptr<Core::IOptionsPage> createAgentsSettingsPage(
    AgentFactory *agentFactory, AgentsPageNavigator *navigator);

} // namespace QodeAssist::Settings
