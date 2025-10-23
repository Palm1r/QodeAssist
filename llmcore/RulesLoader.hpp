/* 
 * Copyright (C) 2025 Petr Mironychev
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

#pragma once

#include <QString>

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::LLMCore {

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

} // namespace QodeAssist::LLMCore
