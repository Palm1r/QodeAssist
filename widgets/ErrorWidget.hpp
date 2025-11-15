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

