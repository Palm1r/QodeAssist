// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderListItem.hpp"

#include <utils/elidinglabel.h>
#include <utils/theme/theme.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QStringList>

#include "ProviderInstance.hpp"
#include "SettingsTheme.hpp"
#include "SettingsUiBuilders.hpp"

namespace QodeAssist::Settings {

ProviderListItem::ProviderListItem(const Providers::ProviderInstance &inst, QWidget *parent)
    : QFrame(parent)
    , m_name(inst.name)
{
    setObjectName(QStringLiteral("ProvListItem"));
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);
    setCursor(Qt::PointingHandCursor);

    m_statusDot = new QLabel(QStringLiteral("●"), this);
    QFont df = m_statusDot->font();
    df.setPixelSize(11);
    m_statusDot->setFont(df);
    m_statusDot->setStyleSheet(QStringLiteral("color: %1;").arg(statusColor(Status::Unknown)));

    m_nameLabel = new QLabel(this);
    QFont nf = m_nameLabel->font();
    nf.setBold(true);
    nf.setPixelSize(12);
    m_nameLabel->setFont(nf);
    m_nameLabel->setTextFormat(Qt::RichText);

    m_urlLabel = new Utils::ElidingLabel(this);
    m_urlLabel->setElideMode(Qt::ElideMiddle);
    m_urlLabel->setFont(monospaceFont(10));
    m_urlLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QPalette up = m_urlLabel->palette();
    up.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_urlLabel->setPalette(up);
    m_urlLabel->setText(inst.url);
    m_urlLabel->setVisible(!inst.url.isEmpty());

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(6);
    row->addWidget(m_statusDot, 0, Qt::AlignVCenter);
    row->addWidget(m_nameLabel, 0);
    row->addWidget(m_urlLabel, 1);

    QStringList tipLines;
    if (!inst.description.isEmpty())
        tipLines << inst.description;
    tipLines << tr("API: %1").arg(inst.clientApi);
    if (!inst.url.isEmpty())
        tipLines << tr("URL: %1").arg(inst.url);
    setToolTip(tipLines.join(QStringLiteral("\n")));

    updateNameText();
    applyTheme();
}

void ProviderListItem::setStatus(Status s)
{
    m_status = s;
    m_statusDot->setStyleSheet(QStringLiteral("color: %1;").arg(statusColor(s)));
}

void ProviderListItem::setSelected(bool s)
{
    if (m_selected == s)
        return;
    m_selected = s;
    applyTheme();
}

void ProviderListItem::setFilterHighlight(const QString &lowerFilter)
{
    if (m_filter == lowerFilter)
        return;
    m_filter = lowerFilter;
    updateNameText();
}

void ProviderListItem::updateNameText()
{
    m_nameLabel->setText(filterHighlightedHtml(m_name, m_filter));
}

void ProviderListItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_name);
    QFrame::mouseReleaseEvent(event);
}

void ProviderListItem::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

QString ProviderListItem::statusColor(Status s)
{
    switch (s) {
    case Status::Ok:
        return cssColor(Utils::creatorColor(Utils::Theme::IconsRunColor));
    case Status::Fail:
        return cssColor(Utils::creatorColor(Utils::Theme::TextColorError));
    case Status::Unknown:
        return cssColor(Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    }
    return cssColor(Utils::creatorColor(Utils::Theme::PanelTextColorMid));
}

void ProviderListItem::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);
    const QString accent = m_selected ? cssColor(Utils::creatorColor(Utils::Theme::TextColorLink))
                                      : QStringLiteral("transparent");
    setStyleSheet(QStringLiteral(
                      "#ProvListItem { background:transparent;"
                      " border-top:1px solid %1; border-left:3px solid %2; }")
                      .arg(cssColor(Utils::creatorColor(Utils::Theme::SplitterColor)), accent));
    updateNameText();
}

} // namespace QodeAssist::Settings
