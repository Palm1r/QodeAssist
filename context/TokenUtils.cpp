/*
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "TokenUtils.hpp"

namespace QodeAssist::Context {

int TokenUtils::estimateTokens(const QString &text)
{
    if (text.isEmpty()) {
        return 0;
    }

    // TODO: need to improve
    return text.length() / 4;
}

int TokenUtils::estimateFileTokens(const Context::ContentFile &file)
{
    int total = 0;

    total += estimateTokens(file.filename);
    total += estimateTokens(file.content);
    total += 5;

    return total;
}

int TokenUtils::estimateFilesTokens(const QList<Context::ContentFile> &files)
{
    int total = 0;
    for (const auto &file : files) {
        total += estimateFileTokens(file);
    }
    return total;
}

} // namespace QodeAssist::Context
