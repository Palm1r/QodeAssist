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

#include "GeneralSettings.hpp"
#include "Logger.hpp"

namespace QodeAssist::Context {

ContextManager &ContextManager::instance()
{
    static ContextManager manager;
    return manager;
}

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

ContentFile ContextManager::createContentFile(const QString &filePath) const
{
    ContentFile contentFile;
    QFileInfo fileInfo(filePath);
    contentFile.filename = fileInfo.fileName();
    contentFile.content = readFile(filePath);
    return contentFile;
}

ProgrammingLanguage ContextManager::getDocumentLanguage(const DocumentInfo &documentInfo)
{
    if (!documentInfo.document) {
        LOG_MESSAGE("Error: Document is not available for" + documentInfo.filePath);
        return Context::ProgrammingLanguage::Unknown;
    }

    return Context::ProgrammingLanguageUtils::fromMimeType(documentInfo.mimeType);
}

bool ContextManager::isSpecifyCompletion(
    const DocumentInfo &documentInfo, const Settings::GeneralSettings &generalSettings)
{
    Context::ProgrammingLanguage documentLanguage = getDocumentLanguage(documentInfo);
    Context::ProgrammingLanguage preset1Language = Context::ProgrammingLanguageUtils::fromString(
        generalSettings.preset1Language.displayForIndex(generalSettings.preset1Language()));

    return generalSettings.specifyPreset1() && documentLanguage == preset1Language;
}

} // namespace QodeAssist::Context
