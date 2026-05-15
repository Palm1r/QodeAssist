// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace QodeAssist::Providers {

// LM Studio presents its OpenAI-compatible API as <host>/v1/..., while the
// OpenAI-style clients expect the base URL to already include /v1. Accept the
// configured URL either with or without the /v1 suffix and return it normalized.
inline QString ensureOpenAIV1Base(const QString &url)
{
    QString base = url.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    if (!base.endsWith(QStringLiteral("/v1")))
        base += QStringLiteral("/v1");
    return base;
}

} // namespace QodeAssist::Providers
