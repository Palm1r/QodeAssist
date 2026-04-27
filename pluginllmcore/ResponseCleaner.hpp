// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QRegularExpression>

namespace QodeAssist::PluginLLMCore {

class ResponseCleaner
{
public:
    static QString clean(const QString &response)
    {
        QString cleaned = removeCodeBlocks(response);
        cleaned = trimWhitespace(cleaned);
        cleaned = removeExplanations(cleaned);
        return cleaned;
    }

private:
    static QString removeCodeBlocks(const QString &text)
    {
        if (!text.contains("```")) {
            return text;
        }

        QRegularExpression codeBlockRegex("```\\w*\\n([\\s\\S]*?)```");
        QRegularExpressionMatch match = codeBlockRegex.match(text);
        if (match.hasMatch()) {
            return match.captured(1);
        }

        int firstFence = text.indexOf("```");
        int lastFence = text.lastIndexOf("```");
        if (firstFence != -1 && lastFence > firstFence) {
            int firstNewLine = text.indexOf('\n', firstFence);
            if (firstNewLine != -1) {
                return text.mid(firstNewLine + 1, lastFence - firstNewLine - 1);
            }
        }

        return text;
    }

    static QString trimWhitespace(const QString &text)
    {
        QString result = text;
        while (result.startsWith('\n') || result.startsWith('\r')) {
            result = result.mid(1);
        }
        while (result.endsWith('\n') || result.endsWith('\r')) {
            result.chop(1);
        }
        return result;
    }

    static QString removeExplanations(const QString &text)
    {
        static const QStringList explanationPrefixes = {
            "here's the", "here is the", "here's", "here is",
            "the refactored", "refactored code:", "code:",
            "i've refactored", "i refactored", "i've changed", "i changed"
        };

        QStringList lines = text.split('\n');
        int startLine = 0;

        for (int i = 0; i < qMin(3, lines.size()); ++i) {
            QString line = lines[i].trimmed().toLower();
            bool isExplanation = false;

            for (const QString &prefix : explanationPrefixes) {
                if (line.startsWith(prefix) || line.contains(prefix + " code")) {
                    isExplanation = true;
                    break;
                }
            }

            if (line.length() < 50 && line.endsWith(':')) {
                isExplanation = true;
            }

            if (isExplanation) {
                startLine = i + 1;
            } else if (!line.isEmpty()) {
                break;
            }
        }

        if (startLine > 0 && startLine < lines.size()) {
            lines = lines.mid(startLine);
            return lines.join('\n');
        }

        return text;
    }
};

} // namespace QodeAssist::PluginLLMCore

