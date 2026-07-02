// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QList>
#include <QPointer>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
QT_END_NAMESPACE

namespace QodeAssist {
class AgentFactory;
class Agent;
} // namespace QodeAssist

namespace QodeAssist::Settings {

class AgentModelDialog : public QDialog
{
    Q_OBJECT
public:
    AgentModelDialog(
        AgentFactory *factory,
        const QString &agentName,
        const QString &currentModel,
        QWidget *parent = nullptr);

    [[nodiscard]] QString selectedModel() const;

private:
    void fetchModels();
    void onModelsFetched();

    AgentFactory *m_factory = nullptr;
    QString m_agentName;

    QLineEdit *m_modelEdit = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_fetchBtn = nullptr;

    QPointer<Agent> m_probe;
    QFutureWatcher<QList<QString>> *m_watcher = nullptr;
};

} // namespace QodeAssist::Settings
