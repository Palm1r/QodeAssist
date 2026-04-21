// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
