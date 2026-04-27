// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QRegularExpression>
#include <QString>

namespace QodeAssist {

class CodeHandler
{
public:
    static QString processText(QString text, QString currentFileName);

    /**
     * Detects language from line, or returns empty string if this was not possible
     */
    static QString detectLanguageFromLine(const QString &line);

    /**
     * Detects language file name, or returns empty string if this was not possible
     */
    static QString detectLanguageFromExtension(const QString &extension);

    /**
     * Detects if text contains code blocks, or returns false if this was not possible
     */
    static bool hasCodeBlocks(const QString &text);

private:
    static QString getCommentPrefix(const QString &language);

    static const QRegularExpression &getFullCodeBlockRegex();
    static const QRegularExpression &getPartialStartBlockRegex();
    static const QRegularExpression &getPartialEndBlockRegex();
};

} // namespace QodeAssist
