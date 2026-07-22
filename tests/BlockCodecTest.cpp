// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "BlockCodecTest.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "session/BlockCodec.hpp"
#include "session/ContentBlock.hpp"
#include "session/FileEditPayload.hpp"
#include "session/PermissionRequest.hpp"

namespace QodeAssist {

void BlockCodecTest::testBlockCodecRejectsForgedMarkers()
{
    const QJsonObject payload{{"key", "value"}};

    for (const QLatin1StringView marker : Session::knownPayloadMarkers()) {
        const QString encoded = Session::encodeMarkerPayload(marker, payload);
        QVERIFY(encoded.startsWith(marker));
        QVERIFY(Session::hasPayloadMarker(marker, encoded));

        const auto decoded = Session::decodeMarkerPayload(marker, encoded);
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, payload);

        QVERIFY(!Session::hasPayloadMarker(marker, QStringLiteral(" ") + encoded));
        QVERIFY(!Session::decodeMarkerPayload(marker, QStringLiteral(" ") + encoded));
        QVERIFY(!Session::decodeMarkerPayload(marker, QStringLiteral("echoed: ") + encoded));
        QVERIFY(!Session::decodeMarkerPayload(marker, QString(marker)));
        QVERIFY(!Session::decodeMarkerPayload(marker, marker + QStringLiteral("not json")));
        QVERIFY(!Session::decodeMarkerPayload(marker, marker + QStringLiteral("[1, 2]")));
    }
}

void BlockCodecTest::testFileEditParsingRequiresTheMarkerAtTheStart()
{
    const QString encoded = Session::encodeFileEditPayload(QJsonObject{{"edit_id", "e1"}});

    QVERIFY(Session::isFileEditPayload(encoded));
    const auto parsed = Session::parseFileEditPayload(encoded);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->value("edit_id").toString(), QString("e1"));

    const QString echoed = QStringLiteral("tool result quoting ") + encoded;
    QVERIFY(!Session::isFileEditPayload(echoed));
    QVERIFY(!Session::parseFileEditPayload(echoed));
}

void BlockCodecTest::testPermissionOptionsCarryAllowsAcrossTheSeam()
{
    Session::PermissionBlock block;
    block.requestId = "p1";
    block.title = "Edit main.cpp";
    block.options
        = {Session::PermissionOption{"a", "Allow", "allow_once"},
           Session::PermissionOption{"b", "Always", "allow_always"},
           Session::PermissionOption{"c", "Deny", "reject_once"}};

    const QString encoded = Session::encodePermissionBlock(block);
    const auto payload = Session::decodeMarkerPayload(Session::permissionPayloadMarker, encoded);
    QVERIFY(payload.has_value());

    const QJsonArray options = payload->value("options").toArray();
    QCOMPARE(options.size(), 3);
    QCOMPARE(options.at(0).toObject().value("allows").toBool(), true);
    QCOMPARE(options.at(1).toObject().value("allows").toBool(), true);
    QCOMPARE(options.at(2).toObject().value("allows").toBool(), false);

    const auto decoded = Session::decodePermissionBlock(encoded);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->options, block.options);
}

} // namespace QodeAssist
