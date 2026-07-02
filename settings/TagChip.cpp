// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TagChip.hpp"

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

TagChip::TagChip(const QString &tag, int count, QWidget *parent)
    : QFrame(parent)
    , m_tag(tag)
{
    setObjectName(QStringLiteral("TagChip"));
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);

    m_label = new QLabel(tag, this);
    m_label->setFont(monospaceFont(11));

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 3, 10, 3);
    row->setSpacing(6);
    row->addWidget(m_label);

    if (count >= 0) {
        m_count = new QLabel(QString::number(count), this);
        QFont cf = m_count->font();
        cf.setPixelSize(10);
        m_count->setFont(cf);
        row->addWidget(m_count);
    }
    applyTheme();
}

void TagChip::setActive(bool on)
{
    if (m_active == on)
        return;
    m_active = on;
    applyTheme();
}

void TagChip::setCount(int count)
{
    if (!m_count)
        return;
    m_count->setText(count >= 0 ? QString::number(count) : QString());
}

void TagChip::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_tag);
    QFrame::mouseReleaseEvent(event);
}

void TagChip::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    if (m_hover)
        return;
    m_hover = true;
    applyTheme();
}

void TagChip::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    if (!m_hover)
        return;
    m_hover = false;
    applyTheme();
}

void TagChip::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (m_inApplyTheme)
        return;
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void TagChip::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);

    const QColor text = Utils::creatorColor(Utils::Theme::TextColorNormal);
    const QColor muted = Utils::creatorColor(Utils::Theme::PanelTextColorMid);
    const QColor line = Utils::creatorColor(Utils::Theme::SplitterColor);

    QString bg;
    QColor border;
    QColor labelColor;
    QColor countColor;
    if (m_active) {
        bg = cssColor(Utils::creatorColor(Utils::Theme::BackgroundColorSelected));
        border = Utils::creatorColor(Utils::Theme::TextColorLink);
        labelColor = text;
        countColor = text;
    } else if (m_hover) {
        bg = cssColor(Utils::creatorColor(Utils::Theme::BackgroundColorHover));
        border = line;
        labelColor = text;
        countColor = muted;
    } else {
        bg = QStringLiteral("transparent");
        border = line;
        labelColor = text;
        countColor = muted;
    }

    setStyleSheet(
        QStringLiteral("#TagChip { background:%1; border:1px solid %2; border-radius:10px; }")
            .arg(bg, cssColor(border)));

    QFont lf = m_label->font();
    lf.setBold(m_active);
    m_label->setFont(lf);

    QPalette lp = m_label->palette();
    lp.setColor(QPalette::WindowText, labelColor);
    m_label->setPalette(lp);

    if (m_count) {
        QPalette cp = m_count->palette();
        cp.setColor(QPalette::WindowText, countColor);
        m_count->setPalette(cp);
    }
}

} // namespace QodeAssist::Settings
