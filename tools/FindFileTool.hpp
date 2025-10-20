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

#include <context/IgnoreManager.hpp>
#include <llmcore/BaseTool.hpp>

namespace QodeAssist::Tools {

class FindFileTool : public LLMCore::BaseTool
{
    Q_OBJECT
public:
    explicit FindFileTool(QObject *parent = nullptr);

    QString name() const override;
    QString stringName() const override;
    QString description() const override;
    QJsonObject getDefinition(LLMCore::ToolSchemaFormat format) const override;
    LLMCore::ToolPermissions requiredPermissions() const override;

    QFuture<QString> executeAsync(const QJsonObject &input = QJsonObject()) override;

private:
    struct FileMatch
    {
        QString absolutePath;
        QString relativePath;
        QString projectName;
        enum MatchType { ExactName, PartialName, PathMatch } matchType;
        
        bool operator<(const FileMatch &other) const
        {
            if (matchType != other.matchType) {
                return matchType < other.matchType;
            }
            return relativePath < other.relativePath;
        }
    };

    QList<FileMatch> findMatchingFiles(const QString &query, int maxResults) const;
    QString formatResults(const QList<FileMatch> &matches, int totalFound, int maxResults) const;
    bool isFileInProject(const QString &filePath) const;
    bool matchesFilePattern(const QString &fileName, const QString &pattern) const;

    static constexpr int DEFAULT_MAX_RESULTS = 50;
    Context::IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Tools
