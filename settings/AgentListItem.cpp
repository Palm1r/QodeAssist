// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentListItem.hpp"

#include "SettingsTheme.hpp"
#include "TagChip.hpp"

#include <utils/theme/theme.h>

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QScopedValueRollback>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

AgentListItem::AgentListItem(const AgentConfig &cfg, QWidget *parent)
    : QFrame(parent)
    , m_name(cfg.name)
{
    setObjectName(QStringLiteral("AgentListItem"));
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);
    setCursor(Qt::PointingHandCursor);

    auto *dot = new QLabel(QStringLiteral("●"), this);
    QFont df = dot->font();
    df.setPixelSize(10);
    dot->setFont(df);
    QPalette dp = dot->palette();
    dp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    dot->setPalette(dp);

    auto *nameLbl = new QLabel(cfg.name, this);
    QFont nf = nameLbl->font();
    nf.setBold(true);
    nf.setPixelSize(12);
    nameLbl->setFont(nf);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);
    headerRow->addWidget(dot, 0, Qt::AlignVCenter);
    headerRow->addWidget(nameLbl, 1);

    auto *col = new QVBoxLayout;
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(2);
    col->addLayout(headerRow);

    m_modelLabel = new QLabel(cfg.model, this);
    m_modelLabel->setFont(monospaceFont(11));
    m_modelLabel->setContentsMargins(16, 0, 0, 0);
    QPalette mp = m_modelLabel->palette();
    mp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_modelLabel->setPalette(mp);
    m_modelLabel->setVisible(!cfg.model.isEmpty());
    col->addWidget(m_modelLabel);

    if (!cfg.tags.isEmpty()) {
        auto *tagsHolder = new QWidget(this);
        auto *tagsLay = new QHBoxLayout(tagsHolder);
        tagsLay->setContentsMargins(16, 2, 0, 0);
        tagsLay->setSpacing(3);
        for (const QString &t : cfg.tags) {
            auto *chip = new TagChip(t, -1, tagsHolder);
            connect(chip, &TagChip::clicked, this, &AgentListItem::tagClicked);
            m_chips.append(chip);
            tagsLay->addWidget(chip);
        }
        tagsLay->addStretch(1);
        col->addWidget(tagsHolder);
    }

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(5, 6, 8, 6);
    outer->setSpacing(0);
    outer->addLayout(col);

    applyTheme();
}

void AgentListItem::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    applyTheme();
}

void AgentListItem::setActiveTags(const QSet<QString> &active)
{
    for (auto *chip : m_chips)
        chip->setActive(active.contains(chip->tag()));
}

void AgentListItem::setModel(const QString &model)
{
    if (!m_modelLabel)
        return;
    m_modelLabel->setText(model);
    m_modelLabel->setVisible(!model.isEmpty());
}

void AgentListItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_name);
    QFrame::mouseReleaseEvent(event);
}

void AgentListItem::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void AgentListItem::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);
    const QString accent = m_selected ? cssColor(Utils::creatorColor(Utils::Theme::TextColorLink))
                                      : QStringLiteral("transparent");
    setStyleSheet(QStringLiteral(
                      "#AgentListItem { background:transparent;"
                      " border-top:1px solid %1; border-left:3px solid %2; }")
                      .arg(cssColor(Utils::creatorColor(Utils::Theme::SplitterColor)), accent));
}

} // namespace QodeAssist::Settings
