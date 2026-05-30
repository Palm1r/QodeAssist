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
    Q_DISABLE_COPY_MOVE(ChatAgentController)

public:
    explicit ChatAgentController(QObject *parent = nullptr);

    void setAgentFactory(AgentFactory *factory);

    [[nodiscard]] QStringList availableAgents() const;
    [[nodiscard]] QString currentAgent() const;
    void setCurrentAgent(const QString &name);

    [[nodiscard]] bool currentSupportsTools() const;

signals:
    void availableAgentsChanged();
    void currentAgentChanged();

private:
    void reload();
    void ensureValidCurrent();
    void applyCurrentAgent(const QString &name);

    QPointer<AgentFactory> m_agentFactory;
    QStringList m_availableAgents;
    QString m_currentAgent;
};

} // namespace QodeAssist::Chat
