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
    // TODO replace to QTextMarkdownImporter after QtC on Qt6.9.0
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
