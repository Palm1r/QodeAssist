// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
