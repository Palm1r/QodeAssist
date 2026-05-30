// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ContextManager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "Logger.hpp"
#include "ProjectScannerQtCreator.hpp"

namespace QodeAssist::Context {

ContextManager::ContextManager(QObject *parent)
    : ContextManager(std::make_unique<ProjectScannerQtCreator>(), parent)
{}

ContextManager::ContextManager(std::unique_ptr<IProjectScanner> scanner, QObject *parent)
    : QObject(parent)
    , m_scanner(std::move(scanner))
{}

ContextManager::~ContextManager() = default;

QString ContextManager::readFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Failed to open file for reading: %1 - %2")
                        .arg(filePath, file.errorString()));
        return QString();
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return content;
}

QList<ContentFile> ContextManager::getContentFiles(const QStringList &filePaths) const
{
    QList<ContentFile> files;
    for (const QString &path : filePaths) {
        if (m_scanner->shouldIgnore(path)) {
            LOG_MESSAGE(QString("Ignoring file in context due to .qodeassistignore: %1").arg(path));
            continue;
        }

        ContentFile contentFile = createContentFile(path);
        files.append(contentFile);
    }
    return files;
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
    Q_UNUSED(documentInfo)
    return false;
}

QString ContextManager::openedFilesContext(const QStringList &excludeFiles) const
{
    QString context = "User files context:\n";

    for (const auto &file : m_scanner->openedTextFiles(excludeFiles)) {
        context += QString("File: %1\n").arg(file.filePath);
        context += file.content;
        context += "\n";
    }

    return context;
}

bool ContextManager::shouldIgnore(const QString &filePath) const
{
    return m_scanner->shouldIgnore(filePath);
}

} // namespace QodeAssist::Context
