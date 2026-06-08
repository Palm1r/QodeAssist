// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TagFilterStrip.hpp"

#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"
#include "TagChip.hpp"

#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPalette>
#include <QScopedValueRollback>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace QodeAssist::Settings {

TagFilterStrip::TagFilterStrip(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("TagStrip"));
    setAutoFillBackground(true);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);
    m_layout->setSpacing(5);
    applyTheme();
}

void TagFilterStrip::setAvailableTags(const QMap<QString, int> &countsByTag)
{
    m_counts = countsByTag;
    QSet<QString> stillExisting;
    for (auto it = m_counts.cbegin(); it != m_counts.cend(); ++it)
        stillExisting.insert(it.key());
    QSet<QString> trimmed;
    for (const QString &t : m_activeTags)
        if (stillExisting.contains(t))
            trimmed.insert(t);
    const bool activeChanged = trimmed != m_activeTags;
    if (activeChanged)
        m_activeTags = trimmed;
    rebuild();
    if (activeChanged)
        emit activeTagsChanged(m_activeTags);
}

void TagFilterStrip::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (m_inApplyTheme)
        return;
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void TagFilterStrip::toggleTag(const QString &tag)
{
    if (m_activeTags.contains(tag))
        m_activeTags.remove(tag);
    else
        m_activeTags.insert(tag);
    refreshActiveStates();
    emit activeTagsChanged(m_activeTags);
}

void TagFilterStrip::refreshActiveStates()
{
    for (auto it = m_chipByTag.cbegin(); it != m_chipByTag.cend(); ++it)
        it.value()->setActive(m_activeTags.contains(it.key()));
}

void TagFilterStrip::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);
    const Theme theme = themeFor(palette());
    setStyleSheet(QStringLiteral("QWidget#TagStrip { background:%1;"
                                 " border-bottom:1px solid %2; }")
                      .arg(theme.listHeaderBg, theme.rowSeparator));
}

void TagFilterStrip::rebuild()
{
    while (auto *item = m_layout->takeAt(0)) {
        if (auto *w = item->widget())
            w->deleteLater();
        if (auto *l = item->layout()) {
            while (auto *sub = l->takeAt(0)) {
                if (auto *sw = sub->widget())
                    sw->deleteLater();
                delete sub;
            }
        }
        delete item;
    }
    m_chipByTag.clear();

    if (m_counts.isEmpty()) {
        setVisible(false);
        return;
    }
    setVisible(true);

    auto *headerLine = new QHBoxLayout;
    headerLine->setContentsMargins(0, 0, 0, 0);
    headerLine->setSpacing(6);
    auto *title = new QLabel(tr("FILTER BY TAG"), this);
    applyMutedSmallCaps(title);
    headerLine->addWidget(title);
    headerLine->addStretch(1);
    if (!m_activeTags.isEmpty()) {
        auto *clear = new QLabel(QStringLiteral("<a href=\"#\">%1</a>").arg(tr("clear")), this);
        connect(clear, &QLabel::linkActivated, this, [this](const QString &) {
            if (m_activeTags.isEmpty())
                return;
            m_activeTags.clear();
            refreshActiveStates();
            emit activeTagsChanged(m_activeTags);
        });
        headerLine->addWidget(clear);
    }
    m_layout->addLayout(headerLine);

    std::vector<std::pair<QString, int>> sorted;
    sorted.reserve(m_counts.size());
    for (auto it = m_counts.cbegin(); it != m_counts.cend(); ++it)
        sorted.emplace_back(it.key(), it.value());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto &a, const auto &b) {
                  if (a.second != b.second)
                      return a.second > b.second;
                  return a.first.localeAwareCompare(b.first) < 0;
              });

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(3);
    grid->setVerticalSpacing(3);
    int col = 0, gridRow = 0;
    for (const auto &[tag, count] : sorted) {
        auto *chip = new TagChip(tag, count, this);
        chip->setActive(m_activeTags.contains(tag));
        connect(chip, &TagChip::clicked, this, &TagFilterStrip::toggleTag);
        grid->addWidget(chip, gridRow, col, Qt::AlignLeft);
        m_chipByTag.insert(tag, chip);
        if (++col >= 4) {
            col = 0;
            ++gridRow;
        }
    }
    grid->setColumnStretch(4, 1);
    m_layout->addLayout(grid);
}

} // namespace QodeAssist::Settings
