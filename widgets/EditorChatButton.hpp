// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace QodeAssist {

class EditorChatButton : public QWidget
{
    Q_OBJECT
public:
    explicit EditorChatButton(QWidget *parent = nullptr);
    ~EditorChatButton() override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QPixmap m_logoPixmap;
    QColor m_textColor;
    QColor m_backgroundColor;
    bool m_isPressed = false;
    bool m_isHovered = false;
};

} // namespace QodeAssist
