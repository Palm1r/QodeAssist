// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentModelDialog.hpp"

#include "SettingsTheme.hpp"

#include <utils/theme/theme.h>

#include <Agent.hpp>
#include <AgentFactory.hpp>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

AgentModelDialog::AgentModelDialog(
    AgentFactory *factory,
    const QString &agentName,
    const QString &currentModel,
    QWidget *parent)
    : QDialog(parent)
    , m_factory(factory)
    , m_agentName(agentName)
{
    setWindowTitle(tr("Select Model"));
    resize(440, 380);

    m_modelEdit = new QLineEdit(currentModel, this);
    m_modelEdit->setFont(monospaceFont(11));
    m_modelEdit->setPlaceholderText(tr("Type a model name or pick one below"));
    m_modelEdit->setClearButtonEnabled(true);

    m_fetchBtn = new QPushButton(tr("Fetch available models"), this);
    m_fetchBtn->setToolTip(tr("Query the agent's provider for its installed models"));

    m_status = new QLabel(this);
    m_status->setFont(monospaceFont(10));
    QPalette sp = m_status->palette();
    sp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_status->setPalette(sp);

    m_list = new QListWidget(this);
    m_list->setFont(monospaceFont(11));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(m_list, &QListWidget::currentTextChanged, this, [this](const QString &text) {
        if (!text.isEmpty())
            m_modelEdit->setText(text);
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { accept(); });
    connect(m_fetchBtn, &QPushButton::clicked, this, [this] { fetchModels(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *fetchRow = new QHBoxLayout;
    fetchRow->setContentsMargins(0, 0, 0, 0);
    fetchRow->setSpacing(8);
    fetchRow->addWidget(m_fetchBtn);
    fetchRow->addWidget(m_status, 1);

    auto *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Model:"), this));
    root->addWidget(m_modelEdit);
    root->addLayout(fetchRow);
    root->addWidget(m_list, 1);
    root->addWidget(buttons);

    fetchModels();
}

QString AgentModelDialog::selectedModel() const
{
    return m_modelEdit->text().trimmed();
}

void AgentModelDialog::fetchModels()
{
    if (!m_factory)
        return;
    if (m_watcher && m_watcher->isRunning())
        return;

    QString err;
    Agent *probe = m_factory->create(m_agentName, this, &err);
    if (!probe) {
        m_status->setText(
            err.isEmpty() ? tr("Provider is not available.")
                          : tr("Provider is not available: %1").arg(err));
        return;
    }
    m_probe = probe;

    if (!m_watcher) {
        m_watcher = new QFutureWatcher<QList<QString>>(this);
        connect(m_watcher, &QFutureWatcher<QList<QString>>::finished, this,
                [this] { onModelsFetched(); });
    }

    m_fetchBtn->setEnabled(false);
    m_status->setText(tr("Loading models…"));
    m_watcher->setFuture(probe->installedModels());
}

void AgentModelDialog::onModelsFetched()
{
    QList<QString> models;
    if (m_watcher->future().resultCount() > 0)
        models = m_watcher->result();

    if (m_probe) {
        m_probe->deleteLater();
        m_probe.clear();
    }

    m_fetchBtn->setEnabled(true);

    const QString keep = m_modelEdit->text();
    {
        QSignalBlocker block(m_list);
        m_list->clear();
        m_list->addItems(models);
    }
    m_modelEdit->setText(keep);

    m_status->setText(
        models.isEmpty()
            ? tr("No models returned — type the model name manually.")
            : tr("%n model(s) available.", nullptr, static_cast<int>(models.size())));
}

} // namespace QodeAssist::Settings
