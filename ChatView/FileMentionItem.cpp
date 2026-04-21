// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FileMentionItem.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

namespace QodeAssist::Chat {

FileMentionItem::FileMentionItem(QQuickItem *parent)
    : QQuickItem(parent)
{}

QVariantList FileMentionItem::searchResults() const
{
    return m_searchResults;
}

int FileMentionItem::currentIndex() const
{
    return m_currentIndex;
}

void FileMentionItem::setCurrentIndex(int index)
{
    if (m_currentIndex == index)
        return;
    m_currentIndex = index;
    emit currentIndexChanged();
}

void FileMentionItem::updateSearch(const QString &query)
{
    m_lastQuery = query;

    QVariantList openFiles = getOpenFiles(query);
    QVariantList projectResults = searchProjectFiles(query);

    QSet<QString> openPaths;
    for (const QVariant &item : std::as_const(openFiles)) {
        const QVariantMap map = item.toMap();
        openPaths.insert(map.value("absolutePath").toString());
    }

    QVariantList combined = openFiles;
    for (const QVariant &item : std::as_const(projectResults)) {
        const QVariantMap map = item.toMap();
        if (!map.value("isProject").toBool()
            && openPaths.contains(map.value("absolutePath").toString()))
            continue;
        combined.append(item);
    }

    m_searchResults = combined;
    m_currentIndex = 0;
    emit searchResultsChanged();
    emit currentIndexChanged();
}

void FileMentionItem::refreshSearch()
{
    if (!m_lastQuery.isNull())
        updateSearch(m_lastQuery);
}

void FileMentionItem::moveUp()
{
    if (m_currentIndex > 0) {
        m_currentIndex--;
        emit currentIndexChanged();
    }
}

void FileMentionItem::moveDown()
{
    if (m_currentIndex < m_searchResults.size() - 1) {
        m_currentIndex++;
        emit currentIndexChanged();
    }
}

void FileMentionItem::selectCurrent()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_searchResults.size())
        return;

    const QVariantMap item = m_searchResults[m_currentIndex].toMap();
    if (item.value("isProject").toBool()) {
        emit projectSelected(item.value("projectName").toString());
    } else {
        emit fileSelected(
            item.value("absolutePath").toString(),
            item.value("relativePath").toString(),
            item.value("projectName").toString());
    }
}

void FileMentionItem::dismiss()
{
    m_searchResults.clear();
    m_currentIndex = 0;
    emit searchResultsChanged();
    emit currentIndexChanged();
    emit dismissed();
}

QVariantMap FileMentionItem::applyCurrentSelection(
    const QString &text, int cursorPosition, bool useTools)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_searchResults.size()) {
        dismiss();
        return {};
    }

    const QString textBefore = text.left(cursorPosition);
    const int atIndex = textBefore.lastIndexOf('@');
    if (atIndex < 0) {
        dismiss();
        return {};
    }

    const QVariantMap item = m_searchResults[m_currentIndex].toMap();
    QString replacement;

    if (item.value("isProject").toBool()) {
        replacement = QStringLiteral("@") + item.value("projectName").toString() + ":";
    } else {
        const QString currentQuery = textBefore.mid(atIndex + 1);
        const QVariantMap result = handleFileSelection(
            item.value("absolutePath").toString(),
            item.value("relativePath").toString(),
            item.value("projectName").toString(),
            currentQuery,
            useTools);

        if (result.value("mode").toString() == "mention")
            replacement = result.value("mentionText").toString();
    }

    const QString newText = text.left(atIndex) + replacement + text.mid(cursorPosition);
    const int newCursorPosition = atIndex + replacement.length();

    dismiss();

    return {{"text", newText}, {"cursorPosition", newCursorPosition}};
}

QVariantMap FileMentionItem::handleFileSelection(
    const QString &absolutePath,
    const QString &relativePath,
    const QString &projectName,
    const QString &currentQuery,
    bool useTools)
{
    QVariantMap result;
    const QString fileName = relativePath.section('/', -1);

    QString mentionKey = fileName;
    const int colonIdx = currentQuery.indexOf(':');
    if (colonIdx > 0) {
        const QString projPrefix = currentQuery.left(colonIdx);
        if (projPrefix.compare(projectName, Qt::CaseInsensitive) == 0)
            mentionKey = projPrefix + ":" + fileName;
    }

    if (useTools) {
        registerMention(mentionKey, absolutePath);
        result["mode"] = QStringLiteral("mention");
        result["mentionText"] = "@" + mentionKey + " ";
    } else {
        emit fileAttachRequested({absolutePath});
        result["mode"] = QStringLiteral("attach");
    }

    return result;
}

void FileMentionItem::registerMention(const QString &mentionKey, const QString &absolutePath)
{
    m_atMentionMap[mentionKey] = absolutePath;
}

void FileMentionItem::clearMentions()
{
    m_atMentionMap.clear();
}

QString FileMentionItem::expandMentions(const QString &text)
{
    QString result = text;

    for (auto it = m_atMentionMap.constBegin(); it != m_atMentionMap.constEnd(); ++it) {
        const QString &mentionKey = it.key();
        const QString &absPath = it.value();
        const QString displayName = mentionKey.section(':', -1);
        const QString escaped = QRegularExpression::escape(mentionKey);

        // @key:N-M -> hyperlink + inline code block
        const QRegularExpression rangeRe("@" + escaped + ":(\\d+)-(\\d+)(?=\\s|$)");
        QRegularExpressionMatchIterator matchIt = rangeRe.globalMatch(result);
        QList<QRegularExpressionMatch> matches;
        while (matchIt.hasNext())
            matches.append(matchIt.next());

        for (int i = matches.size() - 1; i >= 0; --i) {
            const auto &m = matches[i];
            const int startLine = m.captured(1).toInt();
            const int endLine = m.captured(2).toInt();
            const QString ext = fileExtension(absPath);
            const QString snippet = readFileLines(absPath, startLine, endLine);
            const QString replacement
                = QString("[@%1:%2-%3](file://%4)\n```%5\n%6```")
                      .arg(displayName)
                      .arg(startLine)
                      .arg(endLine)
                      .arg(absPath, ext, snippet);
            result.replace(m.capturedStart(), m.capturedLength(), replacement);
        }

        // @key -> hyperlink only
        const QRegularExpression simpleRe("@" + escaped + "(?=\\s|$)");
        result.replace(simpleRe, QString("[@%1](file://%2)").arg(displayName, absPath));
    }
    return result;
}

QVariantList FileMentionItem::searchProjectFiles(const QString &query)
{
    QVariantList results;

    struct FileResult
    {
        QString absolutePath;
        QString relativePath;
        QString projectName;
        int priority;
    };

    const auto allProjects = ProjectExplorer::ProjectManager::projects();

    QString projectFilter;
    QString fileQuery = query;
    const int colonIdx = query.indexOf(':');
    if (colonIdx > 0) {
        const QString prefix = query.left(colonIdx);
        for (auto project : allProjects) {
            if (project && project->displayName().compare(prefix, Qt::CaseInsensitive) == 0) {
                projectFilter = project->displayName();
                fileQuery = query.mid(colonIdx + 1);
                break;
            }
        }
    }

    if (projectFilter.isEmpty() && colonIdx < 0) {
        const QString lowerQ = query.toLower();
        for (auto project : allProjects) {
            if (!project)
                continue;
            const QString name = project->displayName();
            if (query.isEmpty() || name.toLower().startsWith(lowerQ)) {
                QVariantMap item;
                item["absolutePath"] = QString();
                item["relativePath"] = name;
                item["projectName"] = name;
                item["isProject"] = true;
                results.append(item);
            }
        }
    }

    QList<FileResult> candidates;
    const QString lowerFileQuery = fileQuery.toLower();
    const bool emptyFileQuery = fileQuery.isEmpty();

    for (auto project : allProjects) {
        if (!project)
            continue;
        if (!projectFilter.isEmpty() && project->displayName() != projectFilter)
            continue;

        const auto projectFiles = project->files(ProjectExplorer::Project::SourceFiles);
        const QString projectDir = project->projectDirectory().path();
        const QString projectName = project->displayName();

        for (const auto &filePath : projectFiles) {
            const QString absolutePath = filePath.path();
            const QFileInfo fileInfo(absolutePath);
            const QString fileName = fileInfo.fileName();
            const QString relativePath = QDir(projectDir).relativeFilePath(absolutePath);
            const QString lowerFileName = fileName.toLower();
            const QString lowerRelativePath = relativePath.toLower();

            int priority = -1;
            if (emptyFileQuery) {
                priority = 3;
            } else if (lowerFileName == lowerFileQuery) {
                priority = 0;
            } else if (lowerFileName.startsWith(lowerFileQuery)) {
                priority = 1;
            } else if (lowerFileName.contains(lowerFileQuery)) {
                priority = 2;
            } else if (lowerRelativePath.contains(lowerFileQuery)) {
                priority = 3;
            }

            if (priority >= 0)
                candidates.append({absolutePath, relativePath, projectName, priority});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const FileResult &a, const FileResult &b) {
        if (a.priority != b.priority)
            return a.priority < b.priority;
        return a.relativePath < b.relativePath;
    });

    const int maxFiles = qMax(0, 10 - results.size());
    const int count = qMin(candidates.size(), maxFiles);
    for (int i = 0; i < count; i++) {
        QVariantMap item;
        item["absolutePath"] = candidates[i].absolutePath;
        item["relativePath"] = candidates[i].relativePath;
        item["projectName"] = candidates[i].projectName;
        item["isProject"] = false;
        results.append(item);
    }

    return results;
}

QVariantList FileMentionItem::getOpenFiles(const QString &query)
{
    QVariantList results;
    const QString lowerQuery = query.toLower();
    const bool emptyQuery = query.isEmpty();
    QSet<QString> addedPaths;

    auto tryAddDocument = [&](Core::IDocument *document) {
        if (!document)
            return;

        const QString absolutePath = document->filePath().toFSPathString();
        if (absolutePath.isEmpty() || addedPaths.contains(absolutePath))
            return;

        const QFileInfo fileInfo(absolutePath);
        const QString fileName = fileInfo.fileName();
        if (fileName.isEmpty())
            return;

        QString relativePath = absolutePath;
        QString projectName;

        auto project = ProjectExplorer::ProjectManager::projectForFile(document->filePath());
        if (project) {
            projectName = project->displayName();
            relativePath = QDir(project->projectDirectory().path()).relativeFilePath(absolutePath);
        }

        if (!emptyQuery) {
            const QString lowerFileName = fileName.toLower();
            const QString lowerRelativePath = relativePath.toLower();
            if (!lowerFileName.contains(lowerQuery) && !lowerRelativePath.contains(lowerQuery))
                return;
        }

        addedPaths.insert(absolutePath);

        QVariantMap item;
        item["absolutePath"] = absolutePath;
        item["relativePath"] = relativePath;
        item["projectName"] = projectName;
        item["isProject"] = false;
        item["isOpen"] = true;
        results.append(item);
    };

    if (auto current = Core::EditorManager::currentEditor())
        tryAddDocument(current->document());

    for (auto editor : Core::EditorManager::visibleEditors())
        if (editor)
            tryAddDocument(editor->document());

    for (auto document : Core::DocumentModel::openedDocuments())
        tryAddDocument(document);

    return results;
}

QString FileMentionItem::readFileLines(const QString &filePath, int startLine, int endLine)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    QString result;
    int lineNum = 1;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (lineNum >= startLine)
            result += line + '\n';
        if (lineNum >= endLine)
            break;
        ++lineNum;
    }
    return result;
}

QString FileMentionItem::fileExtension(const QString &filePath)
{
    const int dot = filePath.lastIndexOf('.');
    return dot >= 0 ? filePath.mid(dot + 1) : QString();
}

} // namespace QodeAssist::Chat
