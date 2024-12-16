/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "CodeHandler.hpp"
#include <QHash>

namespace QodeAssist {

QString CodeHandler::processText(QString text)
{
    QString result;
    QStringList lines = text.split('\n');
    bool inCodeBlock = false;
    QString pendingComments;
    QString currentLanguage;

    for (const QString &line : lines) {
        if (line.trimmed().startsWith("```")) {
            if (!inCodeBlock) {
                currentLanguage = detectLanguage(line);
            }
            inCodeBlock = !inCodeBlock;
            continue;
        }

        if (inCodeBlock) {
            if (!pendingComments.isEmpty()) {
                QStringList commentLines = pendingComments.split('\n');
                QString commentPrefix = getCommentPrefix(currentLanguage);

                for (const QString &commentLine : commentLines) {
                    if (!commentLine.trimmed().isEmpty()) {
                        result += commentPrefix + " " + commentLine.trimmed() + "\n";
                    } else {
                        result += "\n";
                    }
                }
                pendingComments.clear();
            }
            result += line + "\n";
        } else {
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                pendingComments += trimmed + "\n";
            } else {
                pendingComments += "\n";
            }
        }
    }

    return result;
}

QString CodeHandler::getCommentPrefix(const QString &language)
{
    static const QHash<QString, QString> commentPrefixes
        = {{"python", "#"},  {"py", "#"},          {"lua", "--"},   {"javascript", "//"},
           {"js", "//"},     {"typescript", "//"}, {"ts", "//"},    {"cpp", "//"},
           {"c++", "//"},    {"c", "//"},          {"java", "//"},  {"csharp", "//"},
           {"cs", "//"},     {"php", "//"},        {"ruby", "#"},   {"rb", "#"},
           {"rust", "//"},   {"rs", "//"},         {"go", "//"},    {"swift", "//"},
           {"kotlin", "//"}, {"kt", "//"},         {"scala", "//"}, {"r", "#"},
           {"shell", "#"},   {"bash", "#"},        {"sh", "#"},     {"perl", "#"},
           {"pl", "#"},      {"haskell", "--"},    {"hs", "--"}};

    return commentPrefixes.value(language.toLower(), "//");
}

QString CodeHandler::detectLanguage(const QString &line)
{
    QString trimmed = line.trimmed();
    if (trimmed.length() <= 3) { // Если только ```
        return QString();
    }

    return trimmed.mid(3).trimmed();
}

const QRegularExpression &CodeHandler::getFullCodeBlockRegex()
{
    static const QRegularExpression
        regex(R"(```[\w\s]*\n([\s\S]*?)```)", QRegularExpression::MultilineOption);
    return regex;
}

const QRegularExpression &CodeHandler::getPartialStartBlockRegex()
{
    static const QRegularExpression
        regex(R"(```[\w\s]*\n([\s\S]*?)$)", QRegularExpression::MultilineOption);
    return regex;
}

const QRegularExpression &CodeHandler::getPartialEndBlockRegex()
{
    static const QRegularExpression regex(R"(^([\s\S]*?)```)", QRegularExpression::MultilineOption);
    return regex;
}

} // namespace QodeAssist
