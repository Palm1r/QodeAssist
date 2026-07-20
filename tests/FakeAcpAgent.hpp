// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

#include <LLMQore/AcpTypes.hpp>
#include <LLMQore/RpcTransport.hpp>

namespace QodeAssist {

class FakeAcpAgent : public LLMQore::Rpc::Transport
{
    Q_OBJECT

public:
    explicit FakeAcpAgent(QObject *parent = nullptr)
        : LLMQore::Rpc::Transport(parent)
    {}

    void start() override
    {
        m_open = true;
        setState(State::Connected);
    }

    void stop() override
    {
        if (!m_open)
            return;
        m_open = false;
        setState(State::Disconnected);
        emit closed();
    }

    bool isOpen() const override { return m_open; }

    void send(const QJsonObject &message) override
    {
        const QString method = message.value("method").toString();
        m_methods.append(method);

        if (method == QLatin1String("initialize")) {
            m_clientInfo = message.value("params").toObject().value("clientInfo").toObject();
            reply(message, initializeResult());
        } else if (method == QLatin1String("session/new")) {
            m_newSessionCwd = message.value("params").toObject().value("cwd").toString();
            if (m_requireAuthentication && !m_authenticated)
                replyError(message, -32000, QStringLiteral("Authentication required"));
            else
                reply(message, QJsonObject{{"sessionId", m_sessionId}});
        } else if (method == QLatin1String("authenticate")) {
            m_authenticated = true;
            m_authMethodUsed = message.value("params").toObject().value("methodId").toString();
            reply(message, QJsonObject{});
        } else if (method == QLatin1String("session/prompt")) {
            m_prompts.append(message.value("params").toObject().value("prompt").toArray());
            respondToPrompt(message);
        } else if (method == QLatin1String("session/cancel")) {
            m_cancelled = true;
        }
    }

    void setRequireAuthentication(bool require) { m_requireAuthentication = require; }
    void setSupportsLoadSession(bool supports) { m_loadSession = supports; }
    void setPromptFailure(const QString &error) { m_promptError = error; }
    void setHangOnPrompt(bool hang) { m_hangOnPrompt = hang; }
    void setReplyChunks(const QStringList &chunks) { m_chunks = chunks; }
    void setThoughtChunks(const QStringList &chunks) { m_thoughts = chunks; }

    QStringList methods() const { return m_methods; }
    QString newSessionCwd() const { return m_newSessionCwd; }
    QString authMethodUsed() const { return m_authMethodUsed; }
    QJsonObject clientInfo() const { return m_clientInfo; }
    QList<QJsonArray> prompts() const { return m_prompts; }
    bool wasCancelled() const { return m_cancelled; }

private:
    QJsonObject initializeResult() const
    {
        QJsonObject capabilities{{"loadSession", m_loadSession}};
        QJsonObject result{{"protocolVersion", 1}, {"agentCapabilities", capabilities}};
        result.insert(
            "agentInfo",
            QJsonObject{{"name", QStringLiteral("Fake")}, {"version", QStringLiteral("1.0")}});
        if (m_requireAuthentication) {
            result.insert(
                "authMethods",
                QJsonArray{
                    QJsonObject{{"id", QStringLiteral("login")}, {"name", QStringLiteral("Login")}}});
        }
        return result;
    }

    void respondToPrompt(const QJsonObject &request)
    {
        for (const QString &chunk : m_thoughts)
            notifyUpdate(QStringLiteral("agent_thought_chunk"), chunk);
        for (const QString &chunk : m_chunks)
            notifyUpdate(QStringLiteral("agent_message_chunk"), chunk);

        if (m_hangOnPrompt)
            return;

        if (!m_promptError.isEmpty())
            replyError(request, -32603, m_promptError);
        else
            reply(request, QJsonObject{{"stopReason", QStringLiteral("end_turn")}});
    }

    void notifyUpdate(const QString &kind, const QString &text)
    {
        QJsonObject update{
            {"sessionUpdate", kind},
            {"content", QJsonObject{{"type", QStringLiteral("text")}, {"text", text}}}};
        post(
            QJsonObject{
                {"jsonrpc", QStringLiteral("2.0")},
                {"method", QStringLiteral("session/update")},
                {"params", QJsonObject{{"sessionId", m_sessionId}, {"update", update}}}});
    }

    void reply(const QJsonObject &request, const QJsonObject &result)
    {
        post(
            QJsonObject{
                {"jsonrpc", QStringLiteral("2.0")},
                {"id", request.value("id")},
                {"result", result}});
    }

    void replyError(const QJsonObject &request, int code, const QString &message)
    {
        post(
            QJsonObject{
                {"jsonrpc", QStringLiteral("2.0")},
                {"id", request.value("id")},
                {"error", QJsonObject{{"code", code}, {"message", message}}}});
    }

    void post(const QJsonObject &message)
    {
        QTimer::singleShot(0, this, [this, message]() { emit messageReceived(message); });
    }

    bool m_open = false;
    bool m_hangOnPrompt = false;
    bool m_loadSession = false;
    bool m_requireAuthentication = false;
    bool m_authenticated = false;
    bool m_cancelled = false;
    QString m_sessionId = QStringLiteral("fake-session");
    QString m_promptError;
    QString m_newSessionCwd;
    QString m_authMethodUsed;
    QJsonObject m_clientInfo;
    QStringList m_chunks;
    QStringList m_thoughts;
    QStringList m_methods;
    QList<QJsonArray> m_prompts;
};

} // namespace QodeAssist
