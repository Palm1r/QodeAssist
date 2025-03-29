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
#include "IDocumentReader.hpp"
#include "ProgrammingLanguage.hpp"

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Context {

class IContextManager
{
public:
    virtual ~IContextManager() = default;

    virtual QString readFile(const QString &filePath) const = 0;
    virtual QList<ContentFile> getContentFiles(const QStringList &filePaths) const = 0;
    virtual QStringList getProjectSourceFiles(ProjectExplorer::Project *project) const = 0;
    virtual ContentFile createContentFile(const QString &filePath) const = 0;

    virtual ProgrammingLanguage getDocumentLanguage(const DocumentInfo &documentInfo) const = 0;
    virtual bool isSpecifyCompletion(const DocumentInfo &documentInfo) const = 0;
};

} // namespace QodeAssist::Context
