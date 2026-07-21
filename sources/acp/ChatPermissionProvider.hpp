// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>

#include <QString>

#include <LLMQore/AcpPermissionProvider.hpp>

#include "session/TurnLedger.hpp"

namespace QodeAssist::Acp {

class ChatPermissionProvider : public LLMQore::Acp::AcpPermissionProvider
{
    Q_OBJECT

public:
    using RequestHandler = std::function<void(
        const QString &requestId,
        const LLMQore::Acp::ToolCall &toolCall,
        const QList<LLMQore::Acp::PermissionOption> &options)>;

    explicit ChatPermissionProvider(Session::TurnLedger *ledger, QObject *parent = nullptr);

    void setRequestHandler(RequestHandler handler);

    QFuture<LLMQore::Acp::RequestPermissionResult> requestPermission(
        const QString &sessionId,
        const LLMQore::Acp::ToolCall &toolCall,
        const QList<LLMQore::Acp::PermissionOption> &options) override;

private:
    RequestHandler m_requestHandler;
    Session::TurnLedger *m_ledger = nullptr;
};

} // namespace QodeAssist::Acp
