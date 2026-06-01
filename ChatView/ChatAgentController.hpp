// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Chat {

class ChatAgentController : public QObject
{
    Q_OBJECT

public:
    explicit ChatAgentController(QObject *parent = nullptr);

    void setAgentFactory(AgentFactory *factory);

    QStringList availableAgents() const;
    QString currentAgent() const;
    void setCurrentAgent(const QString &name);

    bool currentSupportsThinking() const;
    bool currentSupportsTools() const;

    void reload();

signals:
    void availableAgentsChanged();
    void currentAgentChanged();

private:
    void ensureValidCurrent();

    QPointer<AgentFactory> m_agentFactory;
    QStringList m_availableAgents;
    QString m_currentAgent;
};

} // namespace QodeAssist::Chat
