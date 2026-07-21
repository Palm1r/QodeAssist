// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/FencedText.hpp"

namespace QodeAssist::Session {

namespace {

constexpr int minimumFenceLength = 3;

int longestBacktickRun(const QString &content)
{
    int longest = 0;
    int current = 0;

    for (const QChar character : content) {
        current = character == QLatin1Char('`') ? current + 1 : 0;
        longest = qMax(longest, current);
    }

    return longest;
}

} // namespace

QString fencedFileBlock(const QString &fileName, const QString &content)
{
    const int fenceLength = qMax(minimumFenceLength, longestBacktickRun(content) + 1);
    const QString fence(fenceLength, QLatin1Char('`'));

    return QStringLiteral("File: %1\n%2\n%3\n%2").arg(fileName, fence, content);
}

} // namespace QodeAssist::Session
