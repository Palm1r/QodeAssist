// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ModelSelector.hpp"

#include "SettingsTheme.hpp"

#include <Agent.hpp>
#include <AgentFactory.hpp>

#include <QAbstractItemView>
#include <QCompleter>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStandardItemModel>

namespace QodeAssist::Settings {

ModelSelector::ModelSelector(QWidget *parent)
    : QComboBox(parent)
{
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(24);
    setFont(monospaceFont(11));
    lineEdit()->setPlaceholderText(tr("(set a model)"));
    lineEdit()->setClearButtonEnabled(true);
    setEnabled(false);

    auto *completer = new QCompleter(model(), this);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    setCompleter(completer);

    connect(this, &QComboBox::activated, this, [this](int) { submitCurrent(); });
    connect(lineEdit(), &QLineEdit::returnPressed, this, [this] { submitCurrent(); });
    connect(
        completer,
        QOverload<const QString &>::of(&QCompleter::activated),
        this,
        [this](const QString &text) {
            setEditText(text);
            submitCurrent();
        },
        Qt::QueuedConnection);
}

void ModelSelector::setAgent(
    AgentFactory *factory,
    const QString &agentName,
    const QString &providerInstance,
    const QString &model)
{
    const bool sameTarget = factory == m_factory && agentName == m_agentName
                            && providerInstance == m_providerInstance;
    m_factory = factory;
    m_agentName = agentName;
    m_providerInstance = providerInstance;
    m_model = model;
    if (!sameTarget) {
        m_fetched = false;
        {
            QSignalBlocker block(this);
            clear();
        }
        emit statusChanged(QString());
    }
    {
        QSignalBlocker block(this);
        setCurrentIndex(findText(model));
        setEditText(model);
    }
    setEnabled(m_factory != nullptr);
}

void ModelSelector::clearAgent()
{
    m_factory = nullptr;
    m_agentName.clear();
    m_providerInstance.clear();
    m_model.clear();
    m_fetched = false;
    {
        QSignalBlocker block(this);
        clear();
        clearEditText();
    }
    setEnabled(false);
    emit statusChanged(QString());
}

void ModelSelector::showPopup()
{
    if (!m_fetched && m_factory && !(m_watcher && m_watcher->isRunning()))
        startFetch();
    QComboBox::showPopup();
}

void ModelSelector::startFetch()
{
    QString err;
    Agent *probe = m_factory->create(m_agentName, this, &err);
    if (!probe) {
        emit statusChanged(
            err.isEmpty() ? tr("Provider is not available.")
                          : tr("Provider is not available: %1").arg(err));
        return;
    }
    m_probe = probe;
    m_fetchAgent = m_agentName;
    m_fetchProvider = m_providerInstance;

    if (!m_watcher) {
        m_watcher = new QFutureWatcher<QList<QString>>(this);
        connect(m_watcher, &QFutureWatcher<QList<QString>>::finished, this, [this] { onFetched(); });
    }

    const QString keep = currentText();
    {
        QSignalBlocker block(this);
        clear();
        addItem(tr("Loading models…"));
        if (auto *itemModel = qobject_cast<QStandardItemModel *>(model()))
            itemModel->item(0)->setEnabled(false);
        setCurrentIndex(-1);
        setEditText(keep);
    }
    emit statusChanged(tr("Loading models…"));
    m_watcher->setFuture(probe->installedModels());
}

void ModelSelector::onFetched()
{
    QList<QString> models;
    if (m_watcher->future().resultCount() > 0)
        models = m_watcher->result();

    if (m_probe) {
        m_probe->deleteLater();
        m_probe.clear();
    }

    if (m_fetchAgent != m_agentName || m_fetchProvider != m_providerInstance)
        return;

    m_fetched = true;
    const QString keep = currentText();
    const bool popupOpen = view() && view()->isVisible();
    {
        QSignalBlocker block(this);
        clear();
        addItems(models);
        setCurrentIndex(findText(keep));
        setEditText(keep);
    }
    emit statusChanged(
        models.isEmpty() ? tr("No models returned — type the model name manually.")
                         : tr("%n model(s) available.", nullptr, static_cast<int>(models.size())));
    if (popupOpen) {
        hidePopup();
        QComboBox::showPopup();
    }
}

void ModelSelector::submitCurrent()
{
    const QString model = currentText().trimmed();
    if (model.isEmpty() || model == m_model)
        return;
    emit modelSubmitted(model);
}

} // namespace QodeAssist::Settings
