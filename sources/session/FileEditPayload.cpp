// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/FileEditPayload.hpp"

#include "session/BlockCodec.hpp"

namespace QodeAssist::Session {

bool isFileEditPayload(const QString &text)
{
    return hasPayloadMarker(fileEditPayloadMarker, text);
}

std::optional<QJsonObject> parseFileEditPayload(const QString &text)
{
    return decodeMarkerPayload(fileEditPayloadMarker, text);
}

QString encodeFileEditPayload(const QJsonObject &payload)
{
    return encodeMarkerPayload(fileEditPayloadMarker, payload);
}

} // namespace QodeAssist::Session
