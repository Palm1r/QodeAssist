// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace QodeAssist::Context {

struct OpenedTextFile
{
    QString filePath;
    QString content;
};

class IProjectScanner
{
public:
    virtual ~IProjectScanner() = default;

    virtual QList<OpenedTextFile> openedTextFiles(const QStringList &excludeFiles = {}) const = 0;
    virtual bool shouldIgnore(const QString &filePath) const = 0;
};

} // namespace QodeAssist::Context
