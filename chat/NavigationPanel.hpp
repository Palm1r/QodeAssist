// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <coreplugin/inavigationwidgetfactory.h>
#include <QObject>

namespace QodeAssist::Chat {

class NavigationPanel : public Core::INavigationWidgetFactory
{
    Q_OBJECT
public:
    explicit NavigationPanel();
    ~NavigationPanel();

    Core::NavigationView createWidget() override;
};

} // namespace QodeAssist::Chat
