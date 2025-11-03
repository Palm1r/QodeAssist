
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

namespace QodeAssist::Context {

/**
 * @brief Utility functions for working with Qt Creator projects
 */
class ProjectUtils
{
public:
    /**
     * @brief Check if a file is part of any open project
     * 
     * Checks if the given file path is either:
     * 1. Explicitly listed in project source files
     * 2. Located within a project directory
     * 
     * @param filePath Absolute or canonical file path to check
     * @return true if file is part of any open project, false otherwise
     */
    static bool isFileInProject(const QString &filePath);

    /**
     * @brief Find a file in open projects by filename
     * 
     * Searches all open projects for a file matching the given filename.
     * If multiple files with the same name exist, returns the first match.
     * 
     * @param filename File name to search for (e.g., "main.cpp")
     * @return Absolute file path if found, empty string otherwise
     */
    static QString findFileInProject(const QString &filename);
};

} // namespace QodeAssist::Context
