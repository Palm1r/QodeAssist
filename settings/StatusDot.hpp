// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPainter>
#include <QWidget>

namespace QodeAssist::Settings {

class StatusDot : public QWidget
{
public:
    explicit StatusDot(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(12, 12);
    }

    void setColor(const QColor &color)
    {
        if (m_color == color)
            return;
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        p.drawEllipse(rect().adjusted(2, 2, -2, -2));
    }

private:
    QColor m_color{Qt::gray};
};

} // namespace QodeAssist::Settings
