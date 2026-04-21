// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPainter>
#include <QQuickItem>

namespace QodeAssist::TaskFlow {

class GridBackground : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY gridColorChanged)

public:
    explicit GridBackground(QQuickItem *parent = nullptr);

    int gridSize() const;
    void setGridSize(int size);

    QColor gridColor() const;
    void setGridColor(const QColor &color);

signals:
    void gridSizeChanged();
    void gridColorChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    int m_gridSize = 20;
    QColor m_gridColor = QColor(128, 128, 128);
};

} // namespace QodeAssist::TaskFlow
