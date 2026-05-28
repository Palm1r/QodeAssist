// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatUtils.h"

#include <QClipboard>
#include <QGuiApplication>

namespace QodeAssist::Chat {

void ChatUtils::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

QString ChatUtils::getSafeMarkdownText(const QString &text) const
{
    if (text.isEmpty()) {
        return text;
    }

    QString safeText;
    safeText.reserve(text.size() + 16);

    bool inFenced = false;
    bool inInline = false;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];

        if (!inInline && i + 2 < text.size()
            && text[i] == '`' && text[i + 1] == '`' && text[i + 2] == '`') {
            safeText.append(QStringLiteral("```"));
            inFenced = !inFenced;
            i += 2;
            continue;
        }

        if (!inFenced && ch == '`') {
            safeText.append(ch);
            inInline = !inInline;
            continue;
        }

        if (!inFenced && !inInline && ch == '<') {
            safeText.append(QStringLiteral("&lt;"));
            continue;
        }

        if (ch.isNull()) {
            safeText.append(' ');
        } else if (ch == '\n' || ch == '\t' || ch == '\r' || ch == ' ') {
            safeText.append(ch);
        } else if (ch.isPrint()) {
            safeText.append(ch);
        } else {
            safeText.append(QChar(0xFFFD));
        }
    }

    return safeText;
}

} // namespace QodeAssist::Chat
