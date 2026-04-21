// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CompletionHintWidget.hpp"

#include <QPainter>

#include <utils/theme/theme.h>

namespace QodeAssist {

CompletionHintWidget::CompletionHintWidget(QWidget *parent, int fontSize)
    : QWidget(parent)
    , m_isHovered(false)
{
    m_accentColor = Utils::creatorTheme()->color(Utils::Theme::TextColorNormal);

    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    
    int triangleSize = qMax(6, fontSize / 2);
    setFixedSize(triangleSize, triangleSize);
}

void CompletionHintWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor triangleColor = m_accentColor;
    triangleColor.setAlpha(m_isHovered ? 255 : 200);
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(triangleColor);
    
    QPolygonF triangle;
    int w = width();
    int h = height();
    
    triangle << QPointF(0, 0)
             << QPointF(0, h)
             << QPointF(w, h / 2.0);
    
    painter.drawPolygon(triangle);
}

void CompletionHintWidget::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = true;
    setCursor(Qt::PointingHandCursor);
    update();
}

void CompletionHintWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_isHovered = false;
    setCursor(Qt::ArrowCursor);
    update();
}

} // namespace QodeAssist

