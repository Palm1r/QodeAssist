// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatPermissionProvider.hpp"

#include <utility>

#include <QUuid>

namespace QodeAssist::Acp {

ChatPermissionProvider::ChatPermissionProvider(QObject *parent)
    : LLMQore::Acp::AcpPermissionProvider(parent)
{}

ChatPermissionProvider::~ChatPermissionProvider()
{
    cancelAll();
}

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

    if (!m_requestHandler) {
        resolve(promise, LLMQore::Acp::RequestPermissionResult::cancelled());
        return promise->future();
    }

    const QString requestId = QStringLiteral("perm-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_pending.insert(requestId, promise);

    QFuture<LLMQore::Acp::RequestPermissionResult> future = promise->future();
    m_requestHandler(requestId, toolCall, options);

    return future;
}

bool ChatPermissionProvider::respond(const QString &requestId, const QString &optionId)
{
    const Promise promise = m_pending.take(requestId);
    if (!promise)
        return false;

    resolve(promise, LLMQore::Acp::RequestPermissionResult::selected(optionId));
    return true;
}

bool ChatPermissionProvider::cancel(const QString &requestId)
{
    const Promise promise = m_pending.take(requestId);
    if (!promise)
        return false;

    resolve(promise, LLMQore::Acp::RequestPermissionResult::cancelled());
    return true;
}

QStringList ChatPermissionProvider::cancelAll()
{
    const QHash<QString, Promise> pending = std::exchange(m_pending, {});

    for (const Promise &promise : pending)
        resolve(promise, LLMQore::Acp::RequestPermissionResult::cancelled());

    return pending.keys();
}

void ChatPermissionProvider::resolve(
    const Promise &promise, const LLMQore::Acp::RequestPermissionResult &result)
{
    promise->addResult(result);
    promise->finish();
}

} // namespace QodeAssist::Acp
