// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace QodeAssist::Session {

struct ProjectInfo
{
    bool available = false;
    QString displayName;
    QString sourceRoot;
    std::optional<QString> buildDirectory;

    bool operator==(const ProjectInfo &other) const = default;
};

struct InvokedSkill
{
    QString name;
    QString body;

    bool operator==(const InvokedSkill &other) const = default;
};

struct LinkedFile
{
    QString fileName;
    QString content;

    bool operator==(const LinkedFile &other) const = default;
};

struct TurnContext
{
    QString basePrompt;
    std::optional<QString> rolePrompt;
    ProjectInfo project;
    QString projectRules;
    QString alwaysOnSkills;
    QString skillsCatalog;
    QList<InvokedSkill> invokedSkills;
    QList<QString> linkedFilePaths;
    QList<LinkedFile> linkedFiles;

    bool operator==(const TurnContext &other) const = default;
};

QString renderSystemPrompt(const TurnContext &context);

} // namespace QodeAssist::Session
