// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/BlockCodec.hpp"

#include <QJsonDocument>

namespace QodeAssist::Session {

QList<QLatin1StringView> knownPayloadMarkers()
{
    return {payloadMarkers.begin(), payloadMarkers.end()};
}

bool hasPayloadMarker(QLatin1StringView marker, const QString &text)
{
    return text.startsWith(marker);
}

QString encodeMarkerPayload(QLatin1StringView marker, const QJsonObject &payload)
{
    return marker + QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

std::optional<QJsonObject> decodeMarkerPayload(QLatin1StringView marker, const QString &text)
{
    if (!hasPayloadMarker(marker, text))
        return std::nullopt;

    const QJsonDocument document = QJsonDocument::fromJson(text.mid(marker.size()).toUtf8());
    if (!document.isObject())
        return std::nullopt;

    return document.object();
}

} // namespace QodeAssist::Session
