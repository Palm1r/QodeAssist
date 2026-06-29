// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>
#include <QStringList>

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

struct PipelineRosters
{
    // Code completion is auto-routed: the router walks this ordered list at
    // request time and uses the first agent whose match rules fit the file.
    QStringList codeCompletion;
    // Chat is user-driven: this is an unordered allow-list of the agents
    // offered in the chat picker. The user picks; no routing happens.
    QStringList chatAssistant;
    // Compression and quick refactor each use a single fixed agent.
    QString chatCompression;
    QString quickRefactor;

    [[nodiscard]] static PipelineRosters defaults();
};

enum class PipelinesLoadStatus { Ok, FileMissing, ParseError, SchemaError };

struct PipelinesLoadResult
{
    PipelineRosters rosters;
    PipelinesLoadStatus status = PipelinesLoadStatus::Ok;
    QString message;
};

class PipelinesConfig
{
public:
    [[nodiscard]] static QString filePath();

    [[nodiscard]] static PipelinesLoadResult load();

    [[nodiscard]] static PipelinesLoadResult loadCached();

    [[nodiscard]] static bool save(const PipelineRosters &rosters, QString *errorOut = nullptr);

    [[nodiscard]] static bool validate(
        const QodeAssist::AgentFactory &factory, QString *errorOut = nullptr);
};

} // namespace QodeAssist::Settings
