// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NavigationPanel.hpp"

#include "ChatView/ChatWidget.hpp"
#include "ChatView/SessionFileRegistry.hpp"
#include "QodeAssistConstants.hpp"

namespace QodeAssist::Chat {

NavigationPanel::NavigationPanel(QQmlEngine *engine, SessionFileRegistry *sessionFileRegistry)
    : m_engine{engine}
    , m_sessionFileRegistry{sessionFileRegistry}
{
    setDisplayName(tr("QodeAssist Chat"));
    setPriority(500);
    setId(Constants::QODE_ASSIST_CHAT_NAV_ID);
    setActivationSequence(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
}

NavigationPanel::~NavigationPanel() {}

Core::NavigationView NavigationPanel::createWidget()
{
    return {.widget = new ChatWidget{m_engine, m_sessionFileRegistry}};
}

} // namespace QodeAssist::Chat
