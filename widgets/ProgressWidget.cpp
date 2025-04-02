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

#include "ProgressWidget.hpp"

namespace QodeAssist {

ProgressWidget::ProgressWidget(QWidget *parent)
    : QWidget(parent)
{
    m_dotPosition = 0;
    m_timer.setInterval(300);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_dotPosition = (m_dotPosition + 1) % 4;
        update();
    });
    m_timer.start();

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
}

ProgressWidget::~ProgressWidget()
{
    m_timer.stop();
}

void ProgressWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), m_backgroundColor);

    if (!m_logoPixmap.isNull()) {
        QRect logoRect(
            (width() - m_logoPixmap.width()) / 2, 5, m_logoPixmap.width(), m_logoPixmap.height());
        painter.drawPixmap(logoRect, m_logoPixmap);
    }

    int dotSpacing = 6;
    int dotSize = 4;
    int totalDotWidth = 3 * dotSize + 2 * dotSpacing;
    int startX = (width() - totalDotWidth) / 2;
    int dotY = height() - 8;

    for (int i = 0; i < 3; ++i) {
        QColor dotColor = m_textColor;

        if (m_dotPosition == 0) {
            dotColor.setAlpha(128);
        } else {
            if (i == m_dotPosition - 1) {
                dotColor.setAlpha(255);
            } else {
                dotColor.setAlpha(80);
            }
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(dotColor);

        int x = startX + i * (dotSize + dotSpacing);
        painter.drawEllipse(x, dotY, dotSize, dotSize);
    }
}

} // namespace QodeAssist
