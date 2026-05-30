// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace QodeAssist {

struct AgentConfig
{
    static constexpr int kSupportedSchemaVersion = 1;
    int schemaVersion = 1;
    QString name;
    QString description;
    QString providerInstance;
    QString model;
    QString endpoint;
    QString systemPrompt;
    QStringList tags;

    struct Match
    {
        QStringList filePatterns;
        QStringList pathPatterns;
        QStringList projectNames;

        [[nodiscard]] bool isEmpty() const noexcept
        {
            return filePatterns.isEmpty()
                   && pathPatterns.isEmpty()
                   && projectNames.isEmpty();
        }
    };
    Match match;

    bool enableThinking = false;
    bool enableTools = false;
    bool cachePrompt = false;
    QString cacheTtl;
    QStringList cacheBreakpoints;

    QJsonObject body;
    QString extendsName;
    bool abstract = false;
    bool hidden = false;

    QString sourcePath;
    bool isUserSource() const { return !sourcePath.startsWith(QLatin1StringView{":/"}); }

    static QString validate(const AgentConfig &config);
};

} // namespace QodeAssist
