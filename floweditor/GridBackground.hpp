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

#include <QQuickItem>
#include <QPainter>
#include <QColor>

namespace QodeAssist::FlowEditor {

class GridBackground : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY gridColorChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)

public:
    explicit GridBackground(QQuickItem *parent = nullptr);

    int gridSize() const;
    void setGridSize(int size);

    QColor gridColor() const;
    void setGridColor(const QColor &color);

    qreal opacity() const;
    void setOpacity(qreal opacity);

signals:
    void gridSizeChanged();
    void gridColorChanged();
    void opacityChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    int m_gridSize = 20;
    QColor m_gridColor = QColor(128, 128, 128); // Default to mid gray
    qreal m_opacity = 0.3;
};

} // namespace QodeAssist::FlowEditor 