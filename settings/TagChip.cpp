// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TagChip.hpp"

#include "SettingsTheme.hpp"

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
    setCursor(Qt::PointingHandCursor);

    m_label = new QLabel(tag, this);
    m_label->setFont(monospaceFont(11));

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(5, 0, 5, 0);
    row->setSpacing(4);
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

void TagChip::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_tag);
    QFrame::mouseReleaseEvent(event);
}

void TagChip::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void TagChip::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);
    const Theme theme = themeFor(palette());
    const QString text = palette().color(QPalette::WindowText).name();
    const QString mute = palette().color(QPalette::Mid).name();
    const QString border = m_active ? text : theme.rowSeparator;
    const QString bg = m_active ? theme.rowSelectedBg : QStringLiteral("transparent");
    setStyleSheet(QStringLiteral(
                      "#TagChip { background:%1; border:1px solid %2; }")
                      .arg(bg, border));
    QPalette lp = m_label->palette();
    lp.setColor(QPalette::WindowText, m_active ? QColor(text) : QColor(mute));
    m_label->setPalette(lp);
    if (m_count) {
        QPalette cp = m_count->palette();
        cp.setColor(QPalette::WindowText, QColor(mute));
        m_count->setPalette(cp);
    }
}

} // namespace QodeAssist::Settings
