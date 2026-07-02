// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderListItem.hpp"

#include <utils/theme/theme.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QVBoxLayout>

#include "ProviderInstance.hpp"
#include "SettingsTheme.hpp"

namespace QodeAssist::Settings {

ProviderListItem::ProviderListItem(
    const Providers::ProviderInstance &inst, QWidget *parent)
    : QFrame(parent)
    , m_name(inst.name)
{
    setObjectName(QStringLiteral("ProvListItem"));
    setFrameShape(QFrame::NoFrame);
    setAutoFillBackground(true);
    setCursor(Qt::PointingHandCursor);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);
    m_statusDot = new QLabel(QStringLiteral("●"), this);
    QFont df = m_statusDot->font();
    df.setPixelSize(11);
    m_statusDot->setFont(df);
    m_statusDot->setStyleSheet(QStringLiteral("color: %1;").arg(statusColor(Status::Unknown)));
    m_nameLabel = new QLabel(inst.name, this);
    QFont nf = m_nameLabel->font();
    nf.setBold(true);
    nf.setPixelSize(12);
    m_nameLabel->setFont(nf);
    headerRow->addWidget(m_statusDot, 0, Qt::AlignVCenter);
    headerRow->addWidget(m_nameLabel, 1);

    m_urlLabel = new QLabel(inst.url, this);
    m_urlLabel->setFont(monospaceFont(10));
    QPalette up = m_urlLabel->palette();
    up.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    m_urlLabel->setPalette(up);
    m_urlLabel->setContentsMargins(17, 0, 0, 0);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(5, 6, 8, 6);
    outer->setSpacing(2);
    outer->addLayout(headerRow);
    outer->addWidget(m_urlLabel);

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
}

} // namespace QodeAssist::Settings
