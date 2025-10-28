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
#include <QFuture>
#include <QJsonObject>
#include <QObject>

namespace QodeAssist::Tools {

class FindAndReadFileTool : public LLMCore::BaseTool
{
    Q_OBJECT

public:
    explicit FindAndReadFileTool(QObject *parent = nullptr);

    QString name() const override;
    QString stringName() const override;
    QString description() const override;
    QJsonObject getDefinition(LLMCore::ToolSchemaFormat format) const override;
    LLMCore::ToolPermissions requiredPermissions() const override;
    QFuture<QString> executeAsync(const QJsonObject &input) override;

private:
    enum class MatchType { ExactName, PathMatch, PartialName };

    struct FileMatch
    {
        QString absolutePath;
        QString relativePath;
        QString projectName;
        QString content;
        MatchType matchType;
        bool contentRead = false;
        QString error;

        bool operator<(const FileMatch &other) const
        {
            return static_cast<int>(matchType) < static_cast<int>(other.matchType);
        }
    };

    FileMatch findBestMatch(const QString &query, const QString &filePattern, int maxResults);
    void searchInFileSystem(
        const QString &dirPath,
        const QString &query,
        const QString &projectName,
        const QString &projectDir,
        ProjectExplorer::Project *project,
        QList<FileMatch> &matches,
        int maxResults,
        int &currentDepth,
        int maxDepth = 5);
    bool matchesFilePattern(const QString &fileName, const QString &pattern) const;
    QString readFileContent(const QString &filePath) const;
    QString formatResult(const FileMatch &match, bool readContent) const;

    Context::IgnoreManager *m_ignoreManager;
};

} // namespace QodeAssist::Tools
