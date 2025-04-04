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

#include <QObject>
#include <QString>

#include "ContentFile.hpp"
#include "IContextManager.hpp"
#include "ProgrammingLanguage.hpp"

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Context {

class ContextManager : public QObject, public IContextManager
{
    Q_OBJECT

public:
    explicit ContextManager(QObject *parent = nullptr);
    ~ContextManager() override = default;

    QString readFile(const QString &filePath) const override;
    QList<ContentFile> getContentFiles(const QStringList &filePaths) const override;
    QStringList getProjectSourceFiles(ProjectExplorer::Project *project) const override;
    ContentFile createContentFile(const QString &filePath) const override;

    ProgrammingLanguage getDocumentLanguage(const DocumentInfo &documentInfo) const override;
    bool isSpecifyCompletion(const DocumentInfo &documentInfo) const override;
    QList<QPair<QString, QString>> openedFiles(const QStringList excludeFiles = QStringList{}) const;
    QString openedFilesContext(const QStringList excludeFiles = QStringList{});
};

} // namespace QodeAssist::Context
