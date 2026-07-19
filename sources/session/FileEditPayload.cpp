// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/FileEditPayload.hpp"

#include <QJsonDocument>

namespace QodeAssist::Session {

namespace {

QString fileEditMarker()
{
    return QStringLiteral("QODEASSIST_FILE_EDIT:");
}

} // namespace

bool isFileEditPayload(const QString &text)
{
    return text.startsWith(fileEditMarker());
}

std::optional<QJsonObject> parseFileEditPayload(const QString &text)
{
    const QString marker = fileEditMarker();
    const int markerPos = text.indexOf(marker);
    if (markerPos < 0)
        return std::nullopt;

    const int jsonStart = markerPos + marker.length();
    if (jsonStart >= text.length())
        return std::nullopt;

    const QJsonDocument document = QJsonDocument::fromJson(text.mid(jsonStart).toUtf8());
    if (!document.isObject())
        return std::nullopt;

    return document.object();
}

QString encodeFileEditPayload(const QJsonObject &payload)
{
    return fileEditMarker()
           + QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

} // namespace QodeAssist::Session
