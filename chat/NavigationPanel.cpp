// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NavigationPanel.hpp"

#include "ChatView/ChatWidget.hpp"

namespace QodeAssist::Chat {

NavigationPanel::NavigationPanel()
{
    setDisplayName(tr("QodeAssist Chat"));
    setPriority(500);
    setId("QodeAssistChat");
    setActivationSequence(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
}

NavigationPanel::~NavigationPanel() {}

Core::NavigationView NavigationPanel::createWidget()
{
    Core::NavigationView view;
    view.widget = new ChatWidget;

    return view;
}

} // namespace QodeAssist::Chat
