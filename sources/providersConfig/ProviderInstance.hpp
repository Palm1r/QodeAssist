// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <chrono>

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace QodeAssist::Providers {

struct LaunchConfig
{
    QString command;
    QStringList args;
    QString cwd;
    QHash<QString, QString> env;

    QString readyUrl;
    std::chrono::seconds readyTimeout{30};

    bool autoStart = false;

    bool detach = false;

    [[nodiscard]] bool isEmpty() const noexcept { return command.isEmpty(); }
};

struct ProviderInstance
{
    QString name;
    QString clientApi;
    QString description;
    QString url;
    QString apiKeyRef;
    QJsonObject extras;
    LaunchConfig launch;
    QString extendsName;
    bool abstract = false;

    QString sourcePath;
    bool overridesBundled = false;
    [[nodiscard]] bool isUserSource() const
    {
        return !sourcePath.startsWith(QLatin1StringView{":/"});
    }

    [[nodiscard]] static QString validate(
        const ProviderInstance &inst, const QStringList &knownClientApis);

    [[nodiscard]] static QString warnings(const ProviderInstance &inst);
};

} // namespace QodeAssist::Providers
