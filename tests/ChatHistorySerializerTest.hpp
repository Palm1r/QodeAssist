// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class ChatHistorySerializerTest final : public QObject
{
    Q_OBJECT

private slots:
    void testHistorySnapshotUsesCurrentVersion();
    void testHistorySnapshotRoundTrip();
    void testUnsupportedChatVersionIsRejected();
    void testChatFileWithoutMessagesArrayIsRejected();
    void testLegacyChatFileConvertsToHistory();
    void testLegacyToolRowWithoutMetadataKeepsItsText();
    void testChatRowsRoundTripThroughHistory();
    void testCompressedChatShapeReloadsAsOneAssistantRow();
    void testHistoryAppliesToChatModel();
    void testAdjacentThinkingBlocksSurviveReload();
};

} // namespace QodeAssist
