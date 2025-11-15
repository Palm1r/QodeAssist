/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <utils/aspects.h>
#include <utils/layoutbuilder.h>
#include <QPushButton>
#include <QIcon>

class ButtonAspect : public Utils::BaseAspect
{
    Q_OBJECT

public:
    ButtonAspect(Utils::AspectContainer *container = nullptr)
        : Utils::BaseAspect(container)
    {}

    void addToLayoutImpl(Layouting::Layout &parent) override
    {
        auto button = new QPushButton(m_buttonText);
        button->setVisible(m_visible);
        
        if (!m_icon.isNull()) {
            button->setIcon(m_icon);
            button->setText("");  // Clear text if icon is set
        }
        
        if (m_isCompact) {
            button->setMaximumWidth(30);
            button->setToolTip(m_tooltip.isEmpty() ? m_buttonText : m_tooltip);
        }
        
        connect(button, &QPushButton::clicked, this, &ButtonAspect::clicked);
        connect(this, &ButtonAspect::visibleChanged, button, &QPushButton::setVisible);
        parent.addItem(button);
    }

    void updateVisibility(bool visible)
    {
        if (m_visible == visible)
            return;
        m_visible = visible;
        emit visibleChanged(visible);
    }

    QString m_buttonText;
    QIcon m_icon;
    QString m_tooltip;
    bool m_isCompact = false;

signals:
    void clicked();
    void visibleChanged(bool state);

private:
    bool m_visible = true;
};
