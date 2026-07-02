// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProjectScannerQtCreator.hpp"

#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <texteditor/textdocument.h>
#include <utils/filepath.h>

#include "IgnoreManager.hpp"

namespace QodeAssist::Context {

ProjectScannerQtCreator::ProjectScannerQtCreator()
    : m_ignoreManager(std::make_unique<IgnoreManager>())
{}

ProjectScannerQtCreator::~ProjectScannerQtCreator() = default;

QList<OpenedTextFile> ProjectScannerQtCreator::openedTextFiles(const QStringList &excludeFiles) const
{
    QList<OpenedTextFile> files;

    const auto documents = Core::DocumentModel::openedDocuments();
    for (const auto *document : documents) {
        const auto *textDocument = qobject_cast<const TextEditor::TextDocument *>(document);
        if (!textDocument)
            continue;

        const QString filePath = textDocument->filePath().toUrlishString();
        if (excludeFiles.contains(filePath))
            continue;
        if (shouldIgnore(filePath))
            continue;

        files.append({filePath, textDocument->plainText()});
    }

    return files;
}

bool ProjectScannerQtCreator::shouldIgnore(const QString &filePath) const
{
    auto *project = ProjectExplorer::ProjectManager::projectForFile(
        Utils::FilePath::fromString(filePath));
    return project && m_ignoreManager->shouldIgnore(filePath, project);
}

} // namespace QodeAssist::Context
