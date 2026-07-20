// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QLatin1StringView>
#include <QList>
#include <QString>

#include <array>
#include <optional>

namespace QodeAssist::Session {

inline constexpr std::array<QLatin1StringView, 3> payloadMarkers{
    QLatin1StringView{"QODEASSIST_FILE_EDIT:"},
    QLatin1StringView{"QODEASSIST_PERMISSION:"},
    QLatin1StringView{"QODEASSIST_PLAN:"}};

inline constexpr QLatin1StringView fileEditPayloadMarker = payloadMarkers[0];
inline constexpr QLatin1StringView permissionPayloadMarker = payloadMarkers[1];
inline constexpr QLatin1StringView planPayloadMarker = payloadMarkers[2];

QList<QLatin1StringView> knownPayloadMarkers();

bool hasPayloadMarker(QLatin1StringView marker, const QString &text);
QString encodeMarkerPayload(QLatin1StringView marker, const QJsonObject &payload);
std::optional<QJsonObject> decodeMarkerPayload(QLatin1StringView marker, const QString &text);

} // namespace QodeAssist::Session
