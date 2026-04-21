// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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

    /**
     * @brief Get the project root directory
     * 
     * Returns the root directory of the first open project.
     * If multiple projects are open, returns the first one.
     * 
     * @return Absolute path to project root, or empty string if no project is open
     */
    static QString getProjectRoot();
};

} // namespace QodeAssist::Context
