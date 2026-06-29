// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "AgentSkill.hpp"

namespace QodeAssist::Skills {

class SkillsManager : public QObject
{
    Q_OBJECT

public:
    explicit SkillsManager(QObject *parent = nullptr);

    void configure(
        const QString &projectPath,
        const QStringList &globalRoots,
        const QStringList &projectSubdirs);

    void reload();

    QVector<AgentSkill> skills() const;

    std::optional<AgentSkill> findByName(const QString &name) const;

    static QStringList resolveRoots(
        const QString &projectPath,
        const QStringList &globalRoots,
        const QStringList &projectSubdirs);

    QString catalogText() const;

    QString alwaysOnBodies() const;

signals:
    void skillsChanged();

private:
    QString m_projectPath;
    QStringList m_globalRoots;
    QStringList m_projectSubdirs;
    QVector<AgentSkill> m_skills;
};

} // namespace QodeAssist::Skills
