// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
    QString role;
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

    QString messageFormat;
    QJsonObject sampling;
    QJsonObject thinking;
    QString context;
    QString extendsName;
    bool abstract = false;
    bool hidden = false;

    QString sourcePath;
    bool overridesBundled = false;
    bool isUserSource() const { return !sourcePath.startsWith(QLatin1StringView{":/"}); }

    static QString validate(const AgentConfig &config);
};

} // namespace QodeAssist
