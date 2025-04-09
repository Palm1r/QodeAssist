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

#include "EditorChatButton.hpp"

#include <utils/theme/theme.h>
#include <QMouseEvent>
#include <QPainter>

namespace QodeAssist {

EditorChatButton::EditorChatButton(QWidget *parent)
    : QWidget(parent)
{
    m_textColor = Utils::creatorTheme()->color(Utils::Theme::TextColorNormal);
    m_backgroundColor = Utils::creatorTheme()->color(Utils::Theme::BackgroundColorNormal);

    m_logoPixmap = QPixmap(":/resources/images/qoderassist-icon.png");

    if (!m_logoPixmap.isNull()) {
        QImage image = m_logoPixmap.toImage();
        image = image.convertToFormat(QImage::Format_ARGB32);

        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                QColor pixelColor = QColor::fromRgba(image.pixel(x, y));

                int brightness = (pixelColor.red() + pixelColor.green() + pixelColor.blue()) / 3;

                if (brightness > 200) {
                    pixelColor.setAlpha(0);
                    image.setPixelColor(x, y, pixelColor);
                } else if (pixelColor.alpha() > 0) {
                    int alpha = pixelColor.alpha();
                    pixelColor = m_textColor;
                    pixelColor.setAlpha(alpha);
                    image.setPixelColor(x, y, pixelColor);
                }
            }
        }

        m_logoPixmap = QPixmap::fromImage(image);
        m_logoPixmap = m_logoPixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    setFixedSize(40, 40);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Open QodeAssist Chat"));
}

EditorChatButton::~EditorChatButton() = default;

void EditorChatButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor = m_backgroundColor;
    if (m_isPressed) {
        bgColor = bgColor.darker(120);
    } else if (m_isHovered) {
        bgColor = bgColor.lighter(110);
    }
    painter.fillRect(rect(), bgColor);

    QRect buttonRect = rect().adjusted(4, 4, -4, -4);
    painter.setPen(Qt::NoPen);
    QColor buttonBgColor
        = m_isPressed ? Utils::creatorTheme()->color(Utils::Theme::BackgroundColorHover).darker(110)
                      : Utils::creatorTheme()->color(Utils::Theme::BackgroundColorHover);

    if (m_isHovered) {
        buttonBgColor = buttonBgColor.lighter(110);
    }
    painter.setBrush(buttonBgColor);
    painter.drawEllipse(buttonRect);

    if (!m_logoPixmap.isNull()) {
        QRect logoRect(
            (width() - m_logoPixmap.width()) / 2,
            (height() - m_logoPixmap.height()) / 2,
            m_logoPixmap.width(),
            m_logoPixmap.height());
        painter.drawPixmap(logoRect, m_logoPixmap);
    }
}

void EditorChatButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isPressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void EditorChatButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isPressed) {
        m_isPressed = false;
        update();
        if (rect().contains(event->pos())) {
            emit clicked();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void EditorChatButton::enterEvent(QEnterEvent *event)
{
    m_isHovered = true;
    update();
    QWidget::enterEvent(event);
}

void EditorChatButton::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    m_isPressed = false;
    update();
    QWidget::leaveEvent(event);
}

} // namespace QodeAssist
