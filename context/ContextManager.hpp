// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include "ContentFile.hpp"
#include "IContextManager.hpp"
#include "IgnoreManager.hpp"
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

    IgnoreManager *ignoreManager() const;

private:
    IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Context
