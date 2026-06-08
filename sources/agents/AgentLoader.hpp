// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>
#include <QStringList>
#include <vector>

#include "AgentConfig.hpp"

namespace QodeAssist::Agents {

class AgentLoader
{
public:
    struct LoadResult
    {
        std::vector<AgentConfig> configs;
        QStringList errors;
        QStringList warnings;
    };

    static LoadResult load(const QString &qrcPrefix, const QString &userDir);

    static std::optional<AgentConfig> parseFile(
        const QString &path, QString *error, QStringList *warnings = nullptr);
};

} // namespace QodeAssist::Agents
