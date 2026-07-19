// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

#include "session/SessionEvent.hpp"
#include "session/TurnRequest.hpp"

namespace QodeAssist::Session {

class ChatBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void sendTurn(const TurnRequest &request) = 0;
    virtual void cancel() = 0;

signals:
    void sessionEvent(const QodeAssist::Session::SessionEvent &event);
};

} // namespace QodeAssist::Session
