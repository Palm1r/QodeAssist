// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPainter>
#include <QTimer>
#include <QWidget>
#include <functional>

#include <utils/progressindicator.h>
#include <utils/theme/theme.h>

namespace QodeAssist {

class ProgressWidget : public QWidget
{
    Q_OBJECT
public:
    ProgressWidget(QWidget *parent = nullptr);
    ~ProgressWidget();

    void setCancelCallback(std::function<void()> callback);

signals:
    void cancelRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QTimer m_timer;
    int m_dotPosition;
    QColor m_textColor;
    QColor m_backgroundColor;
    QPixmap m_logoPixmap;
    bool m_isHovered;
    std::function<void()> m_cancelCallback;
};

} // namespace QodeAssist
