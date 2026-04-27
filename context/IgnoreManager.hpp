// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Context {

class IgnoreManager : public QObject
{
    Q_OBJECT

public:
    explicit IgnoreManager(QObject *parent = nullptr);
    ~IgnoreManager() override;

    bool shouldIgnore(const QString &filePath, ProjectExplorer::Project *project = nullptr) const;
    void reloadIgnorePatterns(ProjectExplorer::Project *project);
    void removeIgnorePatterns(ProjectExplorer::Project *project);

    void reloadAllPatterns();

private slots:
    void cleanupConnections();

private:
    bool matchesIgnorePatterns(const QString &path, const QStringList &patterns) const;
    bool isPathExcluded(const QString &path, const QStringList &patterns) const;
    bool matchPathWithPattern(const QString &path, const QString &pattern) const;
    QStringList loadIgnorePatterns(ProjectExplorer::Project *project);
    QString ignoreFilePath(ProjectExplorer::Project *project) const;

    QHash<ProjectExplorer::Project *, QStringList> m_projectIgnorePatterns;
    mutable QHash<QString, bool> m_ignoreCache;
    QHash<ProjectExplorer::Project *, QMetaObject::Connection> m_projectConnections;
};

} // namespace QodeAssist::Context
