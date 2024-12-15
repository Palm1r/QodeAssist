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

namespace QodeAssist {

QString CodeHandler::processText(QString text)
{
    return removeCodeBlockWrappers(text);
}

QString CodeHandler::removeCodeBlockWrappers(QString text)
{
    QRegularExpressionMatchIterator matchIterator = getFullCodeBlockRegex().globalMatch(text);
    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        QString codeBlock = match.captured(0);
        QString codeContent = match.captured(1).trimmed();
        text.replace(codeBlock, codeContent);
    }

    QRegularExpressionMatch startMatch = getPartialStartBlockRegex().match(text);
    if (startMatch.hasMatch()) {
        QString partialBlock = startMatch.captured(0);
        QString codeContent = startMatch.captured(1).trimmed();
        text.replace(partialBlock, codeContent);
    }

    QRegularExpressionMatch endMatch = getPartialEndBlockRegex().match(text);
    if (endMatch.hasMatch()) {
        QString partialBlock = endMatch.captured(0);
        QString codeContent = endMatch.captured(1).trimmed();
        text.replace(partialBlock, codeContent);
    }

    return text;
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
