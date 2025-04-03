/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "ContextManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTextStream>

#include "settings/GeneralSettings.hpp"
#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectnodes.h>
#include <texteditor/textdocument.h>

#include "Logger.hpp"

namespace QodeAssist::Context {

ContextManager::ContextManager(QObject *parent)
    : QObject(parent)
{}

QString ContextManager::readFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&file);
    return in.readAll();
}

QList<ContentFile> ContextManager::getContentFiles(const QStringList &filePaths) const
{
    QList<ContentFile> files;
    for (const QString &path : filePaths) {
        ContentFile contentFile = createContentFile(path);
        files.append(contentFile);
    }
    return files;
}

QStringList ContextManager::getProjectSourceFiles(ProjectExplorer::Project *project) const
{
    QStringList sourceFiles;
    if (!project)
        return sourceFiles;

    auto projectNode = project->rootProjectNode();
    if (!projectNode)
        return sourceFiles;

    projectNode->forEachNode(
        [&sourceFiles, this](ProjectExplorer::FileNode *fileNode) {
            if (fileNode /*&& shouldProcessFile(fileNode->filePath().toString())*/) {
                sourceFiles.append(fileNode->filePath().toUrlishString());
            }
        },
        nullptr);

    return sourceFiles;
}

ContentFile ContextManager::createContentFile(const QString &filePath) const
{
    ContentFile contentFile;
    QFileInfo fileInfo(filePath);
    contentFile.filename = fileInfo.fileName();
    contentFile.content = readFile(filePath);
    return contentFile;
}

ProgrammingLanguage ContextManager::getDocumentLanguage(const DocumentInfo &documentInfo) const
{
    if (!documentInfo.document) {
        LOG_MESSAGE("Error: Document is not available for" + documentInfo.filePath);
        return Context::ProgrammingLanguage::Unknown;
    }

    return Context::ProgrammingLanguageUtils::fromMimeType(documentInfo.mimeType);
}

bool ContextManager::isSpecifyCompletion(const DocumentInfo &documentInfo) const
{
    const auto &generalSettings = Settings::generalSettings();

    Context::ProgrammingLanguage documentLanguage = getDocumentLanguage(documentInfo);
    Context::ProgrammingLanguage preset1Language = Context::ProgrammingLanguageUtils::fromString(
        generalSettings.preset1Language.displayForIndex(generalSettings.preset1Language()));

    return generalSettings.specifyPreset1() && documentLanguage == preset1Language;
}

QList<QPair<QString, QString>> ContextManager::openedFiles(const QStringList excludeFiles) const
{
    auto documents = Core::DocumentModel::openedDocuments();

    QList<QPair<QString, QString>> files;

    for (const auto *document : std::as_const(documents)) {
        auto textDocument = qobject_cast<const TextEditor::TextDocument *>(document);
        if (!textDocument)
            continue;

        auto filePath = textDocument->filePath().toUrlishString();
        if (!excludeFiles.contains(filePath)) {
            files.append({filePath, textDocument->plainText()});
        }
    }

    return files;
}

QString ContextManager::openedFilesContext(const QStringList excludeFiles)
{
    QString context = "User files context:\n";

    auto documents = Core::DocumentModel::openedDocuments();

    for (const auto *document : documents) {
        auto textDocument = qobject_cast<const TextEditor::TextDocument *>(document);
        if (!textDocument)
            continue;

        auto filePath = textDocument->filePath().toUrlishString();
        if (excludeFiles.contains(filePath))
            continue;

        context += QString("File: %1\n").arg(filePath);
        context += textDocument->plainText();

        context += "\n";
    }

    return context;
}

} // namespace QodeAssist::Context
