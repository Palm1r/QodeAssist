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
#include <QString>
#include <QList>

namespace ProjectExplorer {
class Project;
}

namespace QodeAssist::Tools {

/**
 * @brief Utility class for file searching and reading operations
 * 
 * Provides common functionality for file operations used by various tools:
 * - Fuzzy file searching with multiple match strategies
 * - File pattern matching (e.g., *.cpp, *.h)
 * - Secure file content reading with project boundary checks
 * - Integration with IgnoreManager for respecting .qodeassistignore
 */
class FileSearchUtils
{
public:
    /**
     * @brief Match quality levels for file search results
     */
    enum class MatchType {
        ExactName,    ///< Exact filename match (highest priority)
        PathMatch,    ///< Query found in relative path
        PartialName   ///< Query found in filename (lowest priority)
    };

    /**
     * @brief Represents a file search result with metadata
     */
    struct FileMatch
    {
        QString absolutePath;   ///< Full absolute path to the file
        QString relativePath;   ///< Path relative to project root
        QString projectName;    ///< Name of the project containing the file
        QString content;        ///< File content (if read)
        MatchType matchType;    ///< Quality of the match
        bool contentRead = false; ///< Whether content has been read
        QString error;          ///< Error message if operation failed

        /**
         * @brief Compare matches by quality (for sorting)
         */
        bool operator<(const FileMatch &other) const
        {
            return static_cast<int>(matchType) < static_cast<int>(other.matchType);
        }
    };

    /**
     * @brief Find the best matching file across all open projects
     * 
     * Search strategy:
     * 1. Check if query is an absolute path
     * 2. Search in project source files (exact, path, partial matches)
     * 3. Search filesystem within project directories (respects .qodeassistignore)
     * 
     * @param query Filename, partial name, or path to search for (case-insensitive)
     * @param filePattern Optional file pattern filter (e.g., "*.cpp", "*.h")
     * @param maxResults Maximum number of candidates to collect
     * @param ignoreManager IgnoreManager instance for filtering files
     * @return Best matching file, or empty FileMatch if not found
     */
    static FileMatch findBestMatch(
        const QString &query,
        const QString &filePattern = QString(),
        int maxResults = 10,
        Context::IgnoreManager *ignoreManager = nullptr);

    /**
     * @brief Check if a filename matches a file pattern
     * 
     * Supports:
     * - Wildcard patterns (*.cpp, *.h)
     * - Exact filename matching
     * - Empty pattern (matches all)
     * 
     * @param fileName File name to check
     * @param pattern Pattern to match against
     * @return true if filename matches pattern
     */
    static bool matchesFilePattern(const QString &fileName, const QString &pattern);

    /**
     * @brief Read file content with security checks
     * 
     * Performs the following checks:
     * - File exists and is readable
     * - Respects project boundary settings (allowAccessOutsideProject)
     * - Logs access to files outside project scope
     * 
     * @param filePath Absolute path to file
     * @return File content as QString, or null QString on error
     */
    static QString readFileContent(const QString &filePath);

    /**
     * @brief Search for files in filesystem directory tree
     * 
     * Recursively searches a directory for files matching the query.
     * Respects .qodeassistignore patterns and depth limits.
     * 
     * @param dirPath Directory to search in
     * @param query Search query (case-insensitive)
     * @param projectName Name of the project for metadata
     * @param projectDir Root directory of the project
     * @param project Project instance for ignore checking
     * @param matches Output list to append matches to
     * @param maxResults Stop after finding this many matches
     * @param currentDepth Current recursion depth (modified during recursion)
     * @param maxDepth Maximum recursion depth
     * @param ignoreManager IgnoreManager instance for filtering files
     */
    static void searchInFileSystem(
        const QString &dirPath,
        const QString &query,
        const QString &projectName,
        const QString &projectDir,
        ProjectExplorer::Project *project,
        QList<FileMatch> &matches,
        int maxResults,
        int &currentDepth,
        int maxDepth = 5,
        Context::IgnoreManager *ignoreManager = nullptr);
};

} // namespace QodeAssist::Tools
