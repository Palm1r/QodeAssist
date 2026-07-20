// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QString>

#include <optional>

#include "session/TurnContext.hpp"
#include "session/TurnContextPorts.hpp"

namespace QodeAssist::Session {

struct TurnContextRequest
{
    QString message;
    QString basePrompt;
    std::optional<QString> rolePrompt;
    TurnContextNeeds needs;
};

class TurnContextBuilder
{
public:
    TurnContextBuilder(const IProjectContextPort &project, const ISkillsContextPort *skills);

    TurnContextBuilder(IProjectContextPort &&, const ISkillsContextPort *) = delete;

    TurnContext build(const TurnContextRequest &request) const;

private:
    const IProjectContextPort &m_project;
    const ISkillsContextPort *m_skills = nullptr;
};

} // namespace QodeAssist::Session
