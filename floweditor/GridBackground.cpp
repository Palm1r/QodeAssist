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

#include "GridBackground.hpp"
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include <QQuickWindow>
#include <QPainter>
#include <QPixmap>

namespace QodeAssist::FlowEditor {

GridBackground::GridBackground(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

int GridBackground::gridSize() const
{
    return m_gridSize;
}

void GridBackground::setGridSize(int size)
{
    if (m_gridSize != size) {
        m_gridSize = size;
        update();
        emit gridSizeChanged();
    }
}

QColor GridBackground::gridColor() const
{
    return m_gridColor;
}

void GridBackground::setGridColor(const QColor &color)
{
    if (m_gridColor != color) {
        m_gridColor = color;
        update();
        emit gridColorChanged();
    }
}

qreal GridBackground::opacity() const
{
    return m_opacity;
}

void GridBackground::setOpacity(qreal opacity)
{
    if (m_opacity != opacity) {
        m_opacity = opacity;
        update();
        emit opacityChanged();
    }
}

QSGNode *GridBackground::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
    }

    // Create a pixmap to draw the grid pattern
    QPixmap pixmap(width(), height());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Set up the pen for grid lines
    QPen pen(m_gridColor);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setOpacity(m_opacity);

    // Draw vertical lines
    for (int x = 0; x < width(); x += m_gridSize) {
        painter.drawLine(x, 0, x, height());
    }

    // Draw horizontal lines
    for (int y = 0; y < height(); y += m_gridSize) {
        painter.drawLine(0, y, width(), y);
    }

    painter.end();

    // Create texture from pixmap
    QSGTexture *texture = window()->createTextureFromImage(pixmap.toImage());
    node->setTexture(texture);
    node->setRect(boundingRect());

    return node;
}

} // namespace QodeAssist::FlowEditor 