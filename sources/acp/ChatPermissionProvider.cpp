// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatPermissionProvider.hpp"

#include <memory>
#include <utility>

#include <QPromise>

namespace QodeAssist::Acp {

ChatPermissionProvider::ChatPermissionProvider(Session::TurnLedger *ledger, QObject *parent)
    : LLMQore::Acp::AcpPermissionProvider(parent)
    , m_ledger(ledger)
{}

void ChatPermissionProvider::setRequestHandler(RequestHandler handler)
{
    m_requestHandler = std::move(handler);
}

QFuture<LLMQore::Acp::RequestPermissionResult> ChatPermissionProvider::requestPermission(
    const QString &sessionId,
    const LLMQore::Acp::ToolCall &toolCall,
    const QList<LLMQore::Acp::PermissionOption> &options)
{
    Q_UNUSED(sessionId)

    auto promise = std::make_shared<QPromise<LLMQore::Acp::RequestPermissionResult>>();
    promise->start();

    const auto finish = [promise](const LLMQore::Acp::RequestPermissionResult &result) {
        promise->addResult(result);
        promise->finish();
    };

    if (!m_requestHandler) {
        finish(LLMQore::Acp::RequestPermissionResult::cancelled());
        return promise->future();
    }

    const QString requestId = m_ledger->registerPermission(
        [finish](const QString &optionId) {
            finish(LLMQore::Acp::RequestPermissionResult::selected(optionId));
        },
        [finish] { finish(LLMQore::Acp::RequestPermissionResult::cancelled()); });

    QFuture<LLMQore::Acp::RequestPermissionResult> future = promise->future();
    m_requestHandler(requestId, toolCall, options);

    return future;
}

} // namespace QodeAssist::Acp
