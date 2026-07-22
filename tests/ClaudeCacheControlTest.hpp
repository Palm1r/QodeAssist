// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QObject>

namespace QodeAssist {

class ClaudeCacheControlTest final : public QObject
{
    Q_OBJECT

private slots:
    void testCacheControlBreakpointWithoutExtendedTtl();
    void testCacheControlBreakpointWithExtendedTtl();
    void testCacheControlSystemAsStringWrappedIntoArray();
    void testCacheControlEmptySystemStringIsNotWrapped();
    void testCacheControlSystemAsArrayMarksLastBlock();
    void testCacheControlToolsLastEntryGetsCacheControl();
    void testCacheControlSingleMessageHistorySkipped();
    void testCacheControlHistoryBreakpointOnSecondToLastMessage();
    void testCacheControlHistoryArrayContentMarksLastBlock();
    void testCacheControlNoSystemNoToolsNoMessagesIsNoop();
    void testCacheControlEmptyToolsArrayIsNoop();

private:
    static QJsonObject expectedEphemeral(bool extendedTtl);
};

} // namespace QodeAssist
