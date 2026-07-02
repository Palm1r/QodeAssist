// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentListPane.hpp"

#include "AgentListItem.hpp"
#include "CollapsibleHeader.hpp"
#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"
#include "TagFilterStrip.hpp"

#include <utils/theme/theme.h>

#include <Agent.hpp>
#include <AgentFactory.hpp>

#include <algorithm>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPalette>
#include <QPointer>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

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

    if (m_factory) {
        connect(m_factory, &AgentFactory::agentModelChanged, this, [this](const QString &name) {
            const AgentConfig *cfg = m_factory->configByName(name);
            if (!cfg)
                return;
            for (auto *item : m_rows)
                if (item->agentName() == name)
                    item->setModel(cfg->model);
        });
    }

    applyFilterHolderTheme();
}

void AgentListPane::selectByName(const QString &name)
{
    if (name.isEmpty())
        return;
    if (m_factory) {
        if (const AgentConfig *cfg = m_factory->configByName(name))
            m_expandedGroups.insert(groupKey(*cfg));
    }
    setCurrentNameInternal(name, false);
    m_notifyOnRebuild = true;
    rebuildList();
    for (auto *item : m_rows) {
        if (item->agentName() == name) {
            QPointer<AgentListItem> guarded(item);
            QTimer::singleShot(0, this, [this, guarded] {
                if (guarded)
                    m_listScroll->ensureWidgetVisible(guarded, 0, 60);
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
    m_notifyOnRebuild = true;
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
    m_filterHolder->setStyleSheet(
        QStringLiteral(
            "QWidget#FilterHolder { background:%1;"
            " border-bottom:1px solid %2; }")
            .arg(
                cssColor(Utils::creatorColor(Utils::Theme::BackgroundColorNormal)),
                cssColor(Utils::creatorColor(Utils::Theme::SplitterColor))));
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
    const bool filtersActive = !lowerFilter.isEmpty() || !activeTags.isEmpty();
    auto addAgents = [&](const std::vector<const AgentConfig *> &agents) {
        for (const AgentConfig *cfg : agents) {
            auto *item = new AgentListItem(*cfg, content);
            item->setSelected(cfg->name == m_currentName);
            item->setActiveTags(activeTags);
            connect(item, &AgentListItem::clicked, this, &AgentListPane::onRowClicked);
            connect(
                item,
                &AgentListItem::tagClicked,
                this,
                [this](const QString &tag) { m_tagStrip->toggleTag(tag); },
                Qt::QueuedConnection);
            contentLayout->addWidget(item);
            newRows.append(item);
        }
    };
    QSet<QString> liveKeys;
    auto addSection = [&](const QString &title,
                          const QString &sectionKey,
                          const std::vector<const AgentConfig *> &agents) {
        if (agents.empty())
            return;
        contentLayout->addWidget(makeSectionHeader(title, content));

        QMap<QString, std::vector<const AgentConfig *>> byProvider;
        for (const AgentConfig *cfg : agents)
            byProvider[providerLabel(*cfg)].push_back(cfg);
        QStringList providers = byProvider.keys();
        std::sort(providers.begin(), providers.end(), [](const QString &l, const QString &r) {
            return l.localeAwareCompare(r) < 0;
        });

        for (const QString &provider : providers) {
            const std::vector<const AgentConfig *> &group = byProvider[provider];
            const QString key = sectionKey + QLatin1Char('/') + provider;
            liveKeys.insert(key);
            const bool expanded = filtersActive || m_expandedGroups.contains(key);

            auto *header = new CollapsibleHeader(provider, int(group.size()), content);
            header->setExpanded(expanded);
            header->setClickable(!filtersActive);
            if (!filtersActive) {
                connect(
                    header,
                    &CollapsibleHeader::toggled,
                    this,
                    [this, key] {
                        if (!m_expandedGroups.remove(key))
                            m_expandedGroups.insert(key);
                        rebuildList();
                    },
                    Qt::QueuedConnection);
            }
            contentLayout->addWidget(header);

            if (expanded)
                addAgents(group);
        }
    };

    addSection(tr("User"), QStringLiteral("user"), userAgents);
    addSection(tr("Bundled"), QStringLiteral("bundled"), bundledAgents);
    if (!filtersActive)
        m_expandedGroups.intersect(liveKeys);
    if (userAgents.empty() && bundledAgents.empty()) {
        auto *empty = new QLabel(tr("No agents match these filters."), content);
        empty->setAlignment(Qt::AlignCenter);
        empty->setContentsMargins(10, 16, 10, 16);
        QPalette ep = empty->palette();
        ep.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
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
        m_notifyOnRebuild = false;
        setCurrentNameInternal(fallback, true);
        return;
    }
    if (m_notifyOnRebuild) {
        m_notifyOnRebuild = false;
        emit currentAgentChanged(m_currentName);
    }
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

QString AgentListPane::providerLabel(const AgentConfig &a) const
{
    return a.providerInstance.isEmpty() ? tr("Other") : a.providerInstance;
}

QString AgentListPane::groupKey(const AgentConfig &a) const
{
    const QString section = a.isUserSource() ? QStringLiteral("user") : QStringLiteral("bundled");
    return section + QLatin1Char('/') + providerLabel(a);
}

} // namespace QodeAssist::Settings
