// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <memory>

#include "IProjectScanner.hpp"

namespace QodeAssist::Context {

class IgnoreManager;

class ProjectScannerQtCreator : public IProjectScanner
{
public:
    ProjectScannerQtCreator();
    ~ProjectScannerQtCreator() override;

    QList<OpenedTextFile> openedTextFiles(const QStringList &excludeFiles = {}) const override;
    bool shouldIgnore(const QString &filePath) const override;

private:
    std::unique_ptr<IgnoreManager> m_ignoreManager;
};

} // namespace QodeAssist::Context
