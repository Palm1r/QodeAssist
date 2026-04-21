// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "GridBackground.hpp"
#include <QPainter>
#include <QPixmap>
#include <QQuickWindow>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>

namespace QodeAssist::TaskFlow {

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

QSGNode *GridBackground::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
    }

    QPixmap pixmap(width(), height());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QPen pen(m_gridColor);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setOpacity(this->opacity());

    for (int x = 0; x < width(); x += m_gridSize) {
        painter.drawLine(x, 0, x, height());
    }

    for (int y = 0; y < height(); y += m_gridSize) {
        painter.drawLine(0, y, width(), y);
    }

    painter.end();

    QSGTexture *texture = window()->createTextureFromImage(pixmap.toImage());
    node->setTexture(texture);
    node->setRect(boundingRect());

    return node;
}

} // namespace QodeAssist::TaskFlow
