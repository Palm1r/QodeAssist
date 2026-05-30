// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TagFilterStrip.hpp"

#include "SettingsUiBuilders.hpp"
#include "TagChip.hpp"

#include <utils/theme/theme.h>

#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPalette>
#include <QScopedValueRollback>
#include <QScrollArea>
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
    setAvailableTags(countsByTag, m_activeTags);
}

void TagFilterStrip::setAvailableTags(
    const QMap<QString, int> &countsByTag, const QSet<QString> &activeTags)
{
    m_counts = countsByTag;
    QSet<QString> trimmed;
    for (const QString &t : activeTags)
        if (m_counts.contains(t))
            trimmed.insert(t);
    const bool activeChanged = trimmed != m_activeTags;
    if (activeChanged)
        m_activeTags = trimmed;
    rebuild();
    if (activeChanged)
        emit activeTagsChanged(m_activeTags);
}

void TagFilterStrip::setVisibleCounts(const QMap<QString, int> &countsByTag)
{
    for (auto it = m_chipByTag.cbegin(); it != m_chipByTag.cend(); ++it)
        it.value()->setCount(countsByTag.value(it.key(), 0));
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
    const QString bg = Utils::creatorColor(Utils::Theme::BackgroundColorNormal).name();
    const QString line = Utils::creatorColor(Utils::Theme::SplitterColor).name();
    setStyleSheet(QStringLiteral("QWidget#TagStrip { background:%1;"
                                 " border-bottom:1px solid %2; }")
                      .arg(bg, line));
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
    title->setToolTip(tr("Agents must carry every selected tag"));
    headerLine->addWidget(title);
    auto *andHint = new QLabel(tr("match all"), this);
    {
        QFont hf = andHint->font();
        hf.setPointSizeF(hf.pointSizeF() * 0.85);
        andHint->setFont(hf);
        QPalette hp = andHint->palette();
        hp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
        andHint->setPalette(hp);
    }
    headerLine->addWidget(andHint);
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

    auto *gridHost = new QWidget(this);
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(5);
    int col = 0, gridRow = 0;
    for (const auto &[tag, count] : sorted) {
        auto *chip = new TagChip(tag, count, gridHost);
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

    constexpr int kMaxVisibleRows = 4;
    constexpr int kRowSpacing = 5;
    const int rowCount = gridRow + (col > 0 ? 1 : 0);
    if (rowCount > kMaxVisibleRows && !m_chipByTag.isEmpty()) {
        const int chipHeight = std::max(m_chipByTag.cbegin().value()->sizeHint().height(), 18);
        auto *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(gridHost);
        scroll->setMaximumHeight(
            kMaxVisibleRows * chipHeight + (kMaxVisibleRows - 1) * kRowSpacing + 4);
        m_layout->addWidget(scroll);
    } else {
        m_layout->addWidget(gridHost);
    }
}

} // namespace QodeAssist::Settings
