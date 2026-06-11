// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "ContentFile.hpp"
#include "IContextManager.hpp"
#include "IProjectScanner.hpp"
#include "ProgrammingLanguage.hpp"

namespace QodeAssist::Context {

class ContextManager : public QObject, public IContextManager
{
    Q_OBJECT

public:
    explicit ContextManager(QObject *parent = nullptr);
    ContextManager(std::unique_ptr<IProjectScanner> scanner, QObject *parent = nullptr);
    ~ContextManager() override;

    QString readFile(const QString &filePath) const override;
    QList<ContentFile> getContentFiles(const QStringList &filePaths) const override;
    ContentFile createContentFile(const QString &filePath) const override;

    ProgrammingLanguage getDocumentLanguage(const DocumentInfo &documentInfo) const override;
    bool isSpecifyCompletion(const DocumentInfo &documentInfo) const override;

    QString openedFilesContext(const QStringList &excludeFiles = QStringList{}) const;

    bool shouldIgnore(const QString &filePath) const;

private:
    std::unique_ptr<IProjectScanner> m_scanner;
};

} // namespace QodeAssist::Context
