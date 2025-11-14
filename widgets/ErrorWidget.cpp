/* 
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ErrorWidget.hpp"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainterPath>

namespace QodeAssist {

ErrorWidget::ErrorWidget(const QString &errorMessage, QWidget *parent, int autoHideMs)
    : QWidget(parent)
    , m_errorMessage(errorMessage)
    , m_autoHideTimer(nullptr)
    , m_isHovered(false)
{
    setupColors();
    setupIcon();
    
    QFont errorFont = font();
    errorFont.setPointSize(qMax(8, errorFont.pointSize() - 2));
    setFont(errorFont);
    
    setFixedSize(calculateSize());
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    
    if (autoHideMs > 0) {
        m_autoHideTimer = new QTimer(this);
        m_autoHideTimer->setSingleShot(true);
        connect(m_autoHideTimer, &QTimer::timeout, this, [this]() {
            if (!m_isHovered) {
                emit dismissed();
                deleteLater();
            }
        });
        m_autoHideTimer->start(autoHideMs);
    }
}

ErrorWidget::~ErrorWidget()
{
    if (m_autoHideTimer) {
        m_autoHideTimer->stop();
    }
}

void ErrorWidget::setErrorMessage(const QString &message)
{
    m_errorMessage = message;
    QFont smallFont = font();
    smallFont.setPointSize(qMax(8, smallFont.pointSize() - 2));
    setFont(smallFont);
    setFixedSize(calculateSize());
    update();
}

void ErrorWidget::setupColors()
{
    m_textColor = Utils::creatorTheme()->color(Utils::Theme::TextColorNormal);
    m_backgroundColor = Utils::creatorTheme()->color(Utils::Theme::BackgroundColorNormal);
    m_errorColor = Utils::creatorTheme()->color(Utils::Theme::TextColorError);
}

void ErrorWidget::setupIcon()
{
    // Create a smaller error icon (exclamation mark in a circle)
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw circle
    painter.setPen(QPen(m_errorColor, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(1, 1, 16, 16);
    
    // Draw exclamation mark
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_errorColor);
    
    // Vertical bar of exclamation
    painter.drawRect(8, 4, 2, 8);
    
    // Dot of exclamation
    painter.drawRect(8, 13, 2, 2);
    
    m_errorIcon = pixmap;
}

QSize ErrorWidget::calculateSize() const
{
    QFontMetrics fm(font());
    
    // Maximum width for the text area
    const int maxTextWidth = 350;
    const int iconWidth = 18;
    const int padding = 8;
    const int margin = 12;
    
    // Calculate text area with word wrapping
    QRect textRect = fm.boundingRect(
        0, 0, maxTextWidth, 1000,
        Qt::AlignLeft | Qt::TextWordWrap,
        m_errorMessage
    );
    
    // Total width: margin + icon + padding + text + margin
    int totalWidth = margin + iconWidth + padding + textRect.width() + margin;
    
    // Total height: larger of icon or text, plus vertical margins
    int contentHeight = qMax(iconWidth, textRect.height());
    int totalHeight = contentHeight + margin * 2;
    
    return QSize(totalWidth, totalHeight);
}

void ErrorWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // Draw background with border
    QColor bgColor = m_backgroundColor;
    if (m_isHovered) {
        bgColor = bgColor.lighter(110);
    }
    
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
    
    painter.fillPath(path, bgColor);
    painter.setPen(QPen(m_errorColor.lighter(150), 1));
    painter.drawPath(path);
    
    const int iconSize = 18;
    const int padding = 8;
    const int margin = 12;
    
    // Draw error icon
    if (!m_errorIcon.isNull()) {
        QRect iconRect(margin, margin, iconSize, iconSize);
        painter.drawPixmap(iconRect, m_errorIcon);
    }
    
    // Draw error message with word wrap
    painter.setPen(m_textColor);
    QRect textRect = rect().adjusted(
        margin + iconSize + padding,  // left
        margin,                        // top
        -margin,                       // right
        -margin                        // bottom
    );
    
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, m_errorMessage);
}

void ErrorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit dismissed();
        deleteLater();
    }
    QWidget::mousePressEvent(event);
}

void ErrorWidget::enterEvent(QEnterEvent *event)
{
    m_isHovered = true;
    update();
    
    // Stop auto-hide timer while hovering
    if (m_autoHideTimer && m_autoHideTimer->isActive()) {
        m_autoHideTimer->stop();
    }
    
    QWidget::enterEvent(event);
}

void ErrorWidget::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    update();
    
    // Restart auto-hide timer when leaving
    if (m_autoHideTimer) {
        m_autoHideTimer->start(2000); // Give 2 more seconds after leaving
    }
    
    QWidget::leaveEvent(event);
}

} // namespace QodeAssist

