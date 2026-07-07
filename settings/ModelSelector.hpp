// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QComboBox>
#include <QFutureWatcher>
#include <QList>
#include <QPointer>
#include <QString>

namespace QodeAssist {
class Agent;
class AgentFactory;
} // namespace QodeAssist

namespace QodeAssist::Settings {

class ModelSelector : public QComboBox
{
    Q_OBJECT
public:
    explicit ModelSelector(QWidget *parent = nullptr);

    void setAgent(
        AgentFactory *factory,
        const QString &agentName,
        const QString &providerInstance,
        const QString &model);
    void clearAgent();

signals:
    void modelSubmitted(const QString &model);
    void statusChanged(const QString &status);

protected:
    void showPopup() override;

private:
    void startFetch();
    void onFetched();
    void submitCurrent();

    QPointer<AgentFactory> m_factory;
    QPointer<Agent> m_probe;
    QFutureWatcher<QList<QString>> *m_watcher = nullptr;
    QString m_agentName;
    QString m_providerInstance;
    QString m_model;
    QString m_fetchAgent;
    QString m_fetchProvider;
    bool m_fetched = false;
};

} // namespace QodeAssist::Settings
