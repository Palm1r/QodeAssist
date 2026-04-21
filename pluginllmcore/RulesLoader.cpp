// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RulesLoader.hpp"

#include <QDir>
#include <QFile>

#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

namespace QodeAssist::PluginLLMCore {

QString RulesLoader::loadRules(const QString &projectPath, RulesContext context)
{
    if (projectPath.isEmpty()) {
        return QString();
    }

    QString combined;
    QString basePath = projectPath + "/.qodeassist/rules";

    switch (context) {
    case RulesContext::Completions:
        combined += loadAllMarkdownFiles(basePath + "/completions");
        break;
    case RulesContext::Chat:
        combined += loadAllMarkdownFiles(basePath + "/common");
        combined += loadAllMarkdownFiles(basePath + "/chat");
        break;
    case RulesContext::QuickRefactor:
        combined += loadAllMarkdownFiles(basePath + "/common");
        combined += loadAllMarkdownFiles(basePath + "/quickrefactor");
        break;
    }

    return combined;
}

QString RulesLoader::loadRulesForProject(ProjectExplorer::Project *project, RulesContext context)
{
    if (!project) {
        return QString();
    }

    QString projectPath = getProjectPath(project);
    return loadRules(projectPath, context);
}

ProjectExplorer::Project *RulesLoader::getActiveProject()
{
    auto currentEditor = Core::EditorManager::currentEditor();
    if (currentEditor && currentEditor->document()) {
        Utils::FilePath filePath = currentEditor->document()->filePath();
        auto project = ProjectExplorer::ProjectManager::projectForFile(filePath);
        if (project) {
            return project;
        }
    }

    return ProjectExplorer::ProjectManager::startupProject();
}

QString RulesLoader::loadAllMarkdownFiles(const QString &dirPath)
{
    QString combined;
    QDir dir(dirPath);

    if (!dir.exists()) {
        return QString();
    }

    QStringList mdFiles = dir.entryList({"*.md"}, QDir::Files, QDir::Name);

    for (const QString &fileName : mdFiles) {
        QFile file(dir.filePath(fileName));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            combined += file.readAll();
            combined += "\n\n";
        }
    }

    return combined;
}

QString RulesLoader::getProjectPath(ProjectExplorer::Project *project)
{
    if (!project) {
        return QString();
    }

    return project->projectDirectory().toUrlishString();
}

QVector<RuleFileInfo> RulesLoader::getRuleFiles(const QString &projectPath, RulesContext context)
{
    if (projectPath.isEmpty()) {
        return QVector<RuleFileInfo>();
    }

    QVector<RuleFileInfo> result;
    QString basePath = projectPath + "/.qodeassist/rules";

    // Always include common rules
    result.append(collectMarkdownFiles(basePath + "/common", "common"));

    // Add context-specific rules
    switch (context) {
    case RulesContext::Completions:
        result.append(collectMarkdownFiles(basePath + "/completions", "completions"));
        break;
    case RulesContext::Chat:
        result.append(collectMarkdownFiles(basePath + "/chat", "chat"));
        break;
    case RulesContext::QuickRefactor:
        result.append(collectMarkdownFiles(basePath + "/quickrefactor", "quickrefactor"));
        break;
    }

    return result;
}

QVector<RuleFileInfo> RulesLoader::getRuleFilesForProject(
    ProjectExplorer::Project *project, RulesContext context)
{
    if (!project) {
        return QVector<RuleFileInfo>();
    }

    QString projectPath = getProjectPath(project);
    return getRuleFiles(projectPath, context);
}

QString RulesLoader::loadRuleFileContent(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return file.readAll();
}

QVector<RuleFileInfo> RulesLoader::collectMarkdownFiles(
    const QString &dirPath, const QString &category)
{
    QVector<RuleFileInfo> result;
    QDir dir(dirPath);

    if (!dir.exists()) {
        return result;
    }

    QStringList mdFiles = dir.entryList({"*.md"}, QDir::Files, QDir::Name);

    for (const QString &fileName : mdFiles) {
        QString fullPath = dir.filePath(fileName);
        result.append({fullPath, fileName, category});
    }

    return result;
}

} // namespace QodeAssist::PluginLLMCore
