// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QString>

namespace QodeAssist::Settings {

// Maps legacy provider names (used before multi-API providers were introduced
// with parenthetical suffixes) to their current canonical names. Returns the
// input unchanged if it is not a known legacy name.
inline QString migrateProviderName(const QString &oldName)
{
    static const QHash<QString, QString> renames{
        {QStringLiteral("Ollama"), QStringLiteral("Ollama (Native)")},
        {QStringLiteral("Ollama Compatible"), QStringLiteral("Ollama (OpenAI-compatible)")},
        {QStringLiteral("OpenAI"), QStringLiteral("OpenAI (Chat Completions)")},
        {QStringLiteral("OpenAI Responses"), QStringLiteral("OpenAI (Responses API)")},
        {QStringLiteral("LM Studio"), QStringLiteral("LM Studio (Chat Completions)")},
        {QStringLiteral("LM Studio Responses"), QStringLiteral("LM Studio (Responses API)")},
    };

    const auto it = renames.constFind(oldName);
    return it != renames.constEnd() ? it.value() : oldName;
}

} // namespace QodeAssist::Settings
