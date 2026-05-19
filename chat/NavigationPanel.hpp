// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <coreplugin/inavigationwidgetfactory.h>
#include <QObject>
#include <QPointer>

class QQmlEngine;

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Chat {

class SessionFileRegistry;

class NavigationPanel : public Core::INavigationWidgetFactory
{
    Q_OBJECT
public:
    explicit NavigationPanel(
        QQmlEngine *engine,
        SessionFileRegistry *sessionFileRegistry,
        Skills::SkillsManager *skillsManager);
    ~NavigationPanel();

    Core::NavigationView createWidget() override;

private:
    QPointer<QQmlEngine> m_engine;
    QPointer<SessionFileRegistry> m_sessionFileRegistry;
    QPointer<Skills::SkillsManager> m_skillsManager;
};

} // namespace QodeAssist::Chat
