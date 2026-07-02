// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "CollapsibleHeader.hpp"

#include "SettingsTheme.hpp"

#include <utils/theme/theme.h>

#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QScopedValueRollback>

namespace QodeAssist::Settings {

CollapsibleHeader::CollapsibleHeader(const QString &title, int count, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("CollapsibleHeader"));
    setAutoFillBackground(true);
    setCursor(Qt::PointingHandCursor);

    m_arrow = new QLabel(this);
    QFont af = m_arrow->font();
    af.setPixelSize(9);
    m_arrow->setFont(af);
    m_arrow->setFixedWidth(12);
    m_arrow->setAlignment(Qt::AlignCenter);

    m_title = new QLabel(title.toUpper(), this);
    QFont tf = m_title->font();
    tf.setPixelSize(11);
    tf.setBold(true);
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
    m_title->setFont(tf);

    m_count = new QLabel(QString::number(count), this);
    m_count->setFont(monospaceFont(10));

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(16, 5, 10, 5);
    row->setSpacing(6);
    row->addWidget(m_arrow, 0, Qt::AlignVCenter);
    row->addWidget(m_title, 0, Qt::AlignVCenter);
    row->addStretch(1);
    row->addWidget(m_count, 0, Qt::AlignVCenter);

    updateArrow();
    applyTheme();
}

void CollapsibleHeader::setExpanded(bool expanded)
{
    m_expanded = expanded;
    updateArrow();
}

void CollapsibleHeader::setClickable(bool clickable)
{
    m_clickable = clickable;
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void CollapsibleHeader::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_clickable && event->button() == Qt::LeftButton)
        emit toggled();
    QFrame::mouseReleaseEvent(event);
}

void CollapsibleHeader::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    applyTheme();
    QFrame::enterEvent(event);
}

void CollapsibleHeader::leaveEvent(QEvent *event)
{
    m_hovered = false;
    applyTheme();
    QFrame::leaveEvent(event);
}

void CollapsibleHeader::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void CollapsibleHeader::updateArrow()
{
    m_arrow->setText(m_expanded ? QStringLiteral("▾") : QStringLiteral("▸"));
}

void CollapsibleHeader::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);

    const QColor base = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor mid = Utils::creatorColor(Utils::Theme::PanelTextColorMid);
    const QColor bg = mix(base, mid, m_hovered ? 0.18 : 0.08);

    setStyleSheet(QStringLiteral(
                      "#CollapsibleHeader { background:%1;"
                      " border-top:1px solid %2; }")
                      .arg(cssColor(bg), cssColor(mix(base, mid, 0.25))));

    QPalette ap = m_arrow->palette();
    ap.setColor(QPalette::WindowText, mid);
    m_arrow->setPalette(ap);

    QPalette tp = m_title->palette();
    tp.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorLight));
    m_title->setPalette(tp);

    QPalette cp = m_count->palette();
    cp.setColor(QPalette::WindowText, mid);
    m_count->setPalette(cp);
}

} // namespace QodeAssist::Settings
