// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

#include "session/HistoryProjection.hpp"

namespace QodeAssist::Session {
class Session;
}

namespace QodeAssist::Chat {

class ChatModel;

class ChatHistoryBridge : public QObject
{
    Q_OBJECT

public:
    ChatHistoryBridge(Session::Session *session, ChatModel *model, QObject *parent = nullptr);

private:
    void onRowsReset(const QList<Session::MessageRow> &rows);
    void onRowsAppended(const QList<Session::MessageRow> &rows);
    void onRowUpdated(int index, const Session::MessageRow &row);
    void onRowsRemoved(int first, int count);

    QPointer<ChatModel> m_model;
};

} // namespace QodeAssist::Chat
