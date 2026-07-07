// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentListItem.hpp"

#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"

#include <utils/elidinglabel.h>
#include <utils/theme/theme.h>

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QScopedValueRollback>

namespace QodeAssist::Settings {

AgentListItem::AgentListItem(const AgentConfig &cfg, QWidget *parent)
    : QFrame(parent)
    , m_name(cfg.name)
    , m_model(cfg.model)
{
    setObjectName(QStringLiteral("AgentListItem"));
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);
    setCursor(Qt::PointingHandCursor);

    m_nameLabel = new QLabel(this);
    QFont nf = m_nameLabel->font();
    nf.setBold(true);
    nf.setPixelSize(12);
    m_nameLabel->setFont(nf);
    m_nameLabel->setTextFormat(Qt::RichText);

    m_modelLabel = new Utils::ElidingLabel(this);
    m_modelLabel->setElideMode(Qt::ElideMiddle);
    m_modelLabel->setFont(monospaceFont(11));
    m_modelLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QPalette mp = m_modelLabel->palette();
    mp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_modelLabel->setPalette(mp);
    m_modelLabel->setText(cfg.model);
    m_modelLabel->setVisible(!cfg.model.isEmpty());

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(8);
    row->addWidget(m_nameLabel, 0);
    row->addWidget(m_modelLabel, 1);

    QStringList tipLines;
    if (!cfg.description.isEmpty())
        tipLines << cfg.description;
    if (!cfg.model.isEmpty())
        tipLines << tr("Model: %1").arg(cfg.model);
    if (!cfg.tags.isEmpty())
        tipLines << tr("Tags: %1").arg(cfg.tags.join(QStringLiteral(", ")));
    setToolTip(tipLines.join(QStringLiteral("\n")));

    updateNameText();
    applyTheme();
}

void AgentListItem::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    applyTheme();
}

void AgentListItem::setModel(const QString &model)
{
    m_model = model;
    m_modelLabel->setText(model);
    m_modelLabel->setVisible(!model.isEmpty());
}

void AgentListItem::setFilterHighlight(const QString &lowerFilter)
{
    if (m_filter == lowerFilter)
        return;
    m_filter = lowerFilter;
    updateNameText();
}

void AgentListItem::updateNameText()
{
    m_nameLabel->setText(filterHighlightedHtml(m_name, m_filter));
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
    updateNameText();
}

} // namespace QodeAssist::Settings
