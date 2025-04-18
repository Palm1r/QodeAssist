/* 
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

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

    // Публичный метод для перезагрузки всех шаблонов
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
    mutable QHash<QString, bool> m_ignoreCache; // Кэш результатов для повышения производительности
    QHash<ProjectExplorer::Project *, QMetaObject::Connection> m_projectConnections;
};

} // namespace QodeAssist::Context
