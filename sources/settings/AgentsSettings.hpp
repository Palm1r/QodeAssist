// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <utils/aspects.h>

namespace QodeAssist::Settings {

class AgentsSettings : public Utils::AspectContainer
{
public:
    AgentsSettings();

    Utils::StringAspect agentExtraPaths{this};
    Utils::StringAspect agentForwardedVariables{this};
};

AgentsSettings &agentsSettings();

} // namespace QodeAssist::Settings
