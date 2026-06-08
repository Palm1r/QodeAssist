// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "NavigationPanel.hpp"

#include "ChatView/ChatWidget.hpp"
#include "ChatView/SessionFileRegistry.hpp"
#include "QodeAssistConstants.hpp"
#include "sources/skills/SkillsManager.hpp"

namespace QodeAssist::Chat {

NavigationPanel::NavigationPanel(
    QQmlEngine *engine,
    SessionFileRegistry *sessionFileRegistry,
    Skills::SkillsManager *skillsManager)
    : m_engine{engine}
    , m_sessionFileRegistry{sessionFileRegistry}
    , m_skillsManager{skillsManager}
{
    setDisplayName(tr("QodeAssist Chat"));
    setPriority(500);
    setId(Constants::QODE_ASSIST_CHAT_NAV_ID);
    setActivationSequence(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
}

NavigationPanel::~NavigationPanel() {}

Core::NavigationView NavigationPanel::createWidget()
{
    return {.widget = new ChatWidget{m_engine, m_sessionFileRegistry, m_skillsManager}};
}

} // namespace QodeAssist::Chat
