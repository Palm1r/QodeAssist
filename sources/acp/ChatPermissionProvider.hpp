// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>
#include <memory>

#include <QHash>
#include <QPromise>
#include <QString>
#include <QStringList>

#include <LLMQore/AcpPermissionProvider.hpp>

namespace QodeAssist::Acp {

class ChatPermissionProvider : public LLMQore::Acp::AcpPermissionProvider
{
    Q_OBJECT

public:
    using RequestHandler = std::function<void(
        const QString &requestId,
        const LLMQore::Acp::ToolCall &toolCall,
        const QList<LLMQore::Acp::PermissionOption> &options)>;

    explicit ChatPermissionProvider(QObject *parent = nullptr);
    ~ChatPermissionProvider() override;

    void setRequestHandler(RequestHandler handler);

    QFuture<LLMQore::Acp::RequestPermissionResult> requestPermission(
        const QString &sessionId,
        const LLMQore::Acp::ToolCall &toolCall,
        const QList<LLMQore::Acp::PermissionOption> &options) override;

    bool respond(const QString &requestId, const QString &optionId);
    bool cancel(const QString &requestId);
    QStringList cancelAll();

private:
    using Promise = std::shared_ptr<QPromise<LLMQore::Acp::RequestPermissionResult>>;

    void resolve(const Promise &promise, const LLMQore::Acp::RequestPermissionResult &result);

    RequestHandler m_requestHandler;
    QHash<QString, Promise> m_pending;
};

} // namespace QodeAssist::Acp
