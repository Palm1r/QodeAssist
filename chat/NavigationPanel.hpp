/*
 * Copyright (C) 2024 Petr Mironychev
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
