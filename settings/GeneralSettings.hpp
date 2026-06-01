// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include <utils/aspects.h>

#include "ButtonAspect.hpp"

namespace Core {
class IOptionsPage;
}

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

class GeneralSettings : public Utils::AspectContainer
{
public:
    GeneralSettings();

    Utils::BoolAspect enableQodeAssist{this};
    Utils::BoolAspect enableLogging{this};
    Utils::BoolAspect enableCheckUpdate{this};

    Utils::IntegerAspect requestTimeout{this};

    ButtonAspect checkUpdate{this};
    ButtonAspect resetToDefaults{this};

private:
    void setupConnections();
    void resetPageToDefaults();
};

GeneralSettings &generalSettings();

void showSettings(const Utils::Id page);
void showSettings(const Utils::Id page, Utils::Id item);

class AgentsPageNavigator;

class AgentPipelinesPageNavigator : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AgentPipelinesPageNavigator)
public:
    explicit AgentPipelinesPageNavigator(QObject *parent = nullptr);

signals:
    void editAgentRequested(const QString &agentName);
};

std::unique_ptr<Core::IOptionsPage> createAgentPipelinesSettingsPage(
    AgentFactory *agentFactory,
    AgentPipelinesPageNavigator *navigator,
    AgentsPageNavigator *agentsNavigator);

} // namespace QodeAssist::Settings
