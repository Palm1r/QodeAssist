// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPainter>
#include <QTimer>
#include <QWidget>

#include <utils/theme/theme.h>

namespace QodeAssist {

class ErrorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorWidget(const QString &errorMessage, QWidget *parent = nullptr, int autoHideMs = 5000);
    ~ErrorWidget();

    void setErrorMessage(const QString &message);

    QString errorMessage() const { return m_errorMessage; }

signals:
    void dismissed();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_errorMessage;
    QTimer *m_autoHideTimer;
    QColor m_textColor;
    QColor m_backgroundColor;
    QColor m_errorColor;
    QPixmap m_errorIcon;
    bool m_isHovered;
    
    void setupColors();
    void setupIcon();
    QSize calculateSize() const;
};

} // namespace QodeAssist

