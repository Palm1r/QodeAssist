// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

namespace QodeAssist::Acp {

class AgentKnowledgeService
{
public:
    virtual ~AgentKnowledgeService() = default;

    virtual QString start() = 0;
    virtual void stop() = 0;
    virtual QString serverName() const = 0;
};

} // namespace QodeAssist::Acp
