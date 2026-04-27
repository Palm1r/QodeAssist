// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::PluginLLMCore {

enum class RulesContext { Completions, Chat, QuickRefactor };

struct RuleFileInfo
{
    QString filePath;
    QString fileName;
    QString category; // "common", "chat", "completions", "quickrefactor"
};

class RulesLoader
{
public:
    static QString loadRules(const QString &projectPath, RulesContext context);
    static QString loadRulesForProject(ProjectExplorer::Project *project, RulesContext context);
    static ProjectExplorer::Project *getActiveProject();
    
    // New methods for getting rule files info
    static QVector<RuleFileInfo> getRuleFiles(const QString &projectPath, RulesContext context);
    static QVector<RuleFileInfo> getRuleFilesForProject(ProjectExplorer::Project *project, RulesContext context);
    static QString loadRuleFileContent(const QString &filePath);

private:
    static QString loadAllMarkdownFiles(const QString &dirPath);
    static QVector<RuleFileInfo> collectMarkdownFiles(const QString &dirPath, const QString &category);
    static QString getProjectPath(ProjectExplorer::Project *project);
};

} // namespace QodeAssist::PluginLLMCore
