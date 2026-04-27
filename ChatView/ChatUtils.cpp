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

    bool needsSanitization = false;
    for (const QChar &ch : text) {
        if (ch.isNull() || (!ch.isPrint() && ch != '\n' && ch != '\t' && ch != '\r' && ch != ' ')) {
            needsSanitization = true;
            break;
        }
    }

    if (!needsSanitization) {
        return text;
    }

    QString safeText;
    safeText.reserve(text.size());

    for (QChar ch : text) {
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
