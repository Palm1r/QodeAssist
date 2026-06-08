// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentListPane.hpp"

#include "AgentListItem.hpp"
#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"
#include "TagFilterStrip.hpp"

#include <Agent.hpp>
#include <AgentFactory.hpp>

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPalette>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace QodeAssist::Settings {

AgentListPane::AgentListPane(AgentFactory *factory, QWidget *parent)
    : QFrame(parent)
    , m_factory(factory)
{
    setFrameShape(QFrame::StyledPanel);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter agents…"));
    m_filterEdit->setClearButtonEnabled(true);

    auto *filterRow = new QHBoxLayout;
    filterRow->setContentsMargins(6, 6, 6, 6);
    filterRow->addWidget(m_filterEdit, 1);
    m_filterHolder = new QWidget(this);
    m_filterHolder->setObjectName(QStringLiteral("FilterHolder"));
    m_filterHolder->setLayout(filterRow);
    m_filterHolder->setAutoFillBackground(true);

    m_tagStrip = new TagFilterStrip(this);

    m_listScroll = new QScrollArea(this);
    m_listScroll->setWidgetResizable(true);
    m_listScroll->setFrameShape(QFrame::NoFrame);
    m_listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_filterHolder);
    outer->addWidget(m_tagStrip);
    outer->addWidget(m_listScroll, 1);

    m_filterDebounce = new QTimer(this);
    m_filterDebounce->setSingleShot(true);
    m_filterDebounce->setInterval(100);
    connect(m_filterDebounce, &QTimer::timeout, this, &AgentListPane::rebuildList);
    connect(m_filterEdit, &QLineEdit::textChanged, this,
            [this](const QString &) { m_filterDebounce->start(); });

    connect(m_tagStrip, &TagFilterStrip::activeTagsChanged, this,
            [this](const QSet<QString> &) { rebuildList(); },
            Qt::QueuedConnection);

    applyFilterHolderTheme();
}

void AgentListPane::selectByName(const QString &name)
{
    if (name.isEmpty())
        return;
    setCurrentNameInternal(name, false);
    rebuildList();
    for (auto *item : m_rows) {
        if (item->agentName() == name) {
            QTimer::singleShot(0, this, [this, item] {
                m_listScroll->ensureWidgetVisible(item, 0, 60);
            });
            break;
        }
    }
}

void AgentListPane::refresh()
{
    QMap<QString, int> counts;
    for (const auto *a : visibleAgents())
        for (const QString &t : a->tags)
            counts[t] += 1;
    m_tagStrip->setAvailableTags(counts);
    rebuildList();
}

void AgentListPane::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyFilterHolderTheme();
}

void AgentListPane::applyFilterHolderTheme()
{
    if (!m_filterHolder)
        return;
    const Theme theme = themeFor(palette());
    m_filterHolder->setStyleSheet(
        QStringLiteral("QWidget#FilterHolder { background:%1;"
                       " border-bottom:1px solid %2; }")
            .arg(theme.listHeaderBg, theme.rowSeparator));
}

std::vector<const AgentConfig *> AgentListPane::visibleAgents() const
{
    std::vector<const AgentConfig *> out;
    if (!m_factory)
        return out;
    for (const auto &a : m_factory->configs()) {
        if (a.hidden)
            continue;
        out.push_back(&a);
    }
    return out;
}

bool AgentListPane::matchesFilters(const AgentConfig &a, const QString &lowerFilter) const
{
    if (!lowerFilter.isEmpty()
        && !(a.name + QLatin1Char(' ') + a.model).toLower().contains(lowerFilter))
        return false;
    const QSet<QString> &active = m_tagStrip->activeTags();
    for (const QString &t : active)
        if (!a.tags.contains(t))
            return false;
    return true;
}

void AgentListPane::rebuildList()
{
    const QString lowerFilter = m_filterEdit->text().trimmed().toLower();

    std::vector<const AgentConfig *> userAgents;
    std::vector<const AgentConfig *> bundledAgents;
    for (const auto *a : visibleAgents()) {
        if (!matchesFilters(*a, lowerFilter))
            continue;
        if (a->isUserSource())
            userAgents.push_back(a);
        else
            bundledAgents.push_back(a);
    }
    auto byName = [](const AgentConfig *a, const AgentConfig *b) {
        return a->name.localeAwareCompare(b->name) < 0;
    };
    std::sort(userAgents.begin(), userAgents.end(), byName);
    std::sort(bundledAgents.begin(), bundledAgents.end(), byName);

    QList<AgentListItem *> newRows;
    auto *content = new QWidget;
    content->setAutoFillBackground(true);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    const QSet<QString> &activeTags = m_tagStrip->activeTags();
    auto addAgents = [&](const std::vector<const AgentConfig *> &agents) {
        for (const AgentConfig *cfg : agents) {
            auto *item = new AgentListItem(*cfg, content);
            item->setSelected(cfg->name == m_currentName);
            item->setActiveTags(activeTags);
            connect(item, &AgentListItem::clicked, this, &AgentListPane::onRowClicked);
            connect(item, &AgentListItem::tagClicked, this,
                    [this](const QString &) { refresh(); },
                    Qt::QueuedConnection);
            contentLayout->addWidget(item);
            newRows.append(item);
        }
    };

    if (!userAgents.empty()) {
        contentLayout->addWidget(makeSectionHeader(tr("User"), content));
        addAgents(userAgents);
    }
    if (!bundledAgents.empty()) {
        contentLayout->addWidget(makeSectionHeader(tr("Bundled"), content));
        addAgents(bundledAgents);
    }
    if (newRows.isEmpty()) {
        auto *empty = new QLabel(tr("No agents match these filters."), content);
        empty->setAlignment(Qt::AlignCenter);
        empty->setContentsMargins(10, 16, 10, 16);
        QPalette ep = empty->palette();
        ep.setColor(QPalette::WindowText, ep.color(QPalette::Mid));
        empty->setPalette(ep);
        contentLayout->addWidget(empty);
    }
    contentLayout->addStretch(1);

    m_rows = newRows;
    m_listScroll->setWidget(content);

    const AgentConfig *current
        = m_currentName.isEmpty() || !m_factory
              ? nullptr
              : m_factory->configByName(m_currentName);
    if (!current && !m_rows.isEmpty()) {
        const QString fallback = m_rows.front()->agentName();
        m_rows.front()->setSelected(true);
        setCurrentNameInternal(fallback, /*emitSignal*/ true);
        return;
    }
    emit currentAgentChanged(m_currentName);
}

void AgentListPane::onRowClicked(const QString &name)
{
    setCurrentNameInternal(name, /*emitSignal*/ true);
}

void AgentListPane::setCurrentNameInternal(const QString &name, bool emitSignal)
{
    if (name == m_currentName)
        return;
    m_currentName = name;
    for (auto *item : m_rows)
        item->setSelected(item->agentName() == name);
    if (emitSignal)
        emit currentAgentChanged(m_currentName);
}

} // namespace QodeAssist::Settings
