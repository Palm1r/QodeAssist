// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
