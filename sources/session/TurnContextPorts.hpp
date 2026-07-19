// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

#include "session/TurnContext.hpp"

namespace QodeAssist::Session {

class IProjectContextPort
{
    Q_DISABLE_COPY_MOVE(IProjectContextPort)

public:
    IProjectContextPort() = default;
    virtual ~IProjectContextPort() = default;

    virtual ProjectInfo projectInfo() const = 0;
    virtual QString projectRules() const = 0;
};

class ISkillsContextPort
{
    Q_DISABLE_COPY_MOVE(ISkillsContextPort)

public:
    ISkillsContextPort() = default;
    virtual ~ISkillsContextPort() = default;

    virtual QString alwaysOnBodies() const = 0;
    virtual QString catalogText() const = 0;
    virtual std::optional<InvokedSkill> findSkill(const QString &name) const = 0;
};

class ILinkedFilesPort
{
    Q_DISABLE_COPY_MOVE(ILinkedFilesPort)

public:
    ILinkedFilesPort() = default;
    virtual ~ILinkedFilesPort() = default;

    virtual QList<LinkedFile> readFiles(const QList<QString> &paths) const = 0;
};

} // namespace QodeAssist::Session
