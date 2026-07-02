// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>

#include <QPointer>

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

class AgentsPageNavigator;

class GeneralSettings : public Utils::AspectContainer
{
public:
    GeneralSettings();

    void setAgentPipelinesContext(AgentFactory *agentFactory, AgentsPageNavigator *agentsNavigator);

    Utils::BoolAspect enableQodeAssist{this};
    Utils::BoolAspect enableLogging{this};
    Utils::BoolAspect enableCheckUpdate{this};

    Utils::IntegerAspect requestTimeout{this};

    ButtonAspect checkUpdate{this};
    ButtonAspect resetToDefaults{this};

private:
    void setupConnections();
    void resetPageToDefaults();

    QPointer<AgentFactory> m_agentFactory;
    QPointer<AgentsPageNavigator> m_agentsNavigator;
    std::function<void()> m_resetPipelines;
};

GeneralSettings &generalSettings();

void showSettings(const Utils::Id page);
void showSettings(const Utils::Id page, Utils::Id item);

} // namespace QodeAssist::Settings
