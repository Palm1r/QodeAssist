// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

struct PipelineRosters
{
    QStringList codeCompletion;
    QStringList chatAssistant;
    QStringList chatCompression;
    QStringList quickRefactor;

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

    [[nodiscard]] static bool save(const PipelineRosters &rosters, QString *errorOut = nullptr);

    [[nodiscard]] static bool validate(
        const QodeAssist::AgentFactory &factory, QString *errorOut = nullptr);
};

} // namespace QodeAssist::Settings
