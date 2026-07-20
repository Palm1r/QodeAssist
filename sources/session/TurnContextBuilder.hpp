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
    QList<QString> linkedFilePaths;
    TurnContextNeeds needs;
};

class TurnContextBuilder
{
public:
    TurnContextBuilder(
        const IProjectContextPort &project,
        const ISkillsContextPort *skills,
        const ILinkedFilesPort &linkedFiles);

    TurnContextBuilder(IProjectContextPort &&, const ISkillsContextPort *, const ILinkedFilesPort &)
        = delete;
    TurnContextBuilder(const IProjectContextPort &, const ISkillsContextPort *, ILinkedFilesPort &&)
        = delete;

    TurnContext build(const TurnContextRequest &request) const;

private:
    const IProjectContextPort &m_project;
    const ISkillsContextPort *m_skills = nullptr;
    const ILinkedFilesPort &m_linkedFiles;
};

} // namespace QodeAssist::Session
