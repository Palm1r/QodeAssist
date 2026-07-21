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

        if (method.isEmpty() && message.contains("id")) {
            m_permissionOutcomes.append(
                message.value("result").toObject().value("outcome").toObject());
            if (!m_hangOnPrompt)
                finishPrompt();
            return;
        }

        m_methods.append(method);

        if (method == QLatin1String("initialize")) {
            m_clientInfo = message.value("params").toObject().value("clientInfo").toObject();
            reply(message, initializeResult());
        } else if (method == QLatin1String("session/new")) {
            m_newSessionCwd = message.value("params").toObject().value("cwd").toString();
            m_offeredMcpServers = message.value("params").toObject().value("mcpServers").toArray();
            if (m_hangOnNewSession)
                return;
            if (m_requireAuthentication && !m_authenticated) {
                replyError(message, -32000, QStringLiteral("Authentication required"));
            } else {
                if (!m_commandsBeforeSessionReply.isEmpty()) {
                    postUpdate(
                        QJsonObject{
                            {"sessionUpdate", QStringLiteral("available_commands_update")},
                            {"availableCommands", m_commandsBeforeSessionReply}});
                }
                reply(message, QJsonObject{{"sessionId", m_sessionId}});
            }
        } else if (method == QLatin1String("session/load")) {
            m_loadedSessionId = message.value("params").toObject().value("sessionId").toString();
            if (m_loadSessionError.isEmpty())
                reply(message, QJsonObject{{"sessionId", m_loadedSessionId}});
            else
                replyError(message, -32603, m_loadSessionError);
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
    void setSupportsEmbeddedContext(bool supports) { m_embeddedContext = supports; }
    void setSupportsImages(bool supports) { m_images = supports; }
    void setPromptFailure(const QString &error) { m_promptError = error; }
    void setHangOnPrompt(bool hang) { m_hangOnPrompt = hang; }
    void setReplyChunks(const QStringList &chunks) { m_chunks = chunks; }
    void setThoughtChunks(const QStringList &chunks) { m_thoughts = chunks; }

    void setPermissionRequest(const QJsonObject &toolCall, const QJsonArray &options)
    {
        m_permissionToolCall = toolCall;
        m_permissionOptions = options;
    }

    void setFinishPromptWithoutWaiting(bool finish) { m_finishPromptWithoutWaiting = finish; }

    void setToolCallUpdates(const QList<QJsonObject> &updates) { m_toolCalls = updates; }
    void setPlanUpdates(const QList<QJsonArray> &plans) { m_plans = plans; }
    void setAvailableCommandUpdates(const QList<QJsonArray> &updates)
    {
        m_availableCommandUpdates = updates;
    }
    void setCommandsBeforeSessionReply(const QJsonArray &commands)
    {
        m_commandsBeforeSessionReply = commands;
    }
    void setPromptUsage(const QJsonObject &usage) { m_promptUsage = usage; }
    void setContextGauge(const QJsonObject &gauge) { m_contextGauge = gauge; }
    void setSuggestedTitle(const QString &title) { m_suggestedTitle = title; }
    void setLoadSessionError(const QString &error) { m_loadSessionError = error; }
    void setHangOnNewSession(bool hang) { m_hangOnNewSession = hang; }
    void setSupportsHttpMcp(bool supports) { m_httpMcp = supports; }

    QJsonArray offeredMcpServers() const { return m_offeredMcpServers; }

    QString loadedSessionId() const { return m_loadedSessionId; }

    QList<QJsonObject> permissionOutcomes() const { return m_permissionOutcomes; }

    QStringList methods() const { return m_methods; }
    QString newSessionCwd() const { return m_newSessionCwd; }
    QString authMethodUsed() const { return m_authMethodUsed; }
    QJsonObject clientInfo() const { return m_clientInfo; }
    QList<QJsonArray> prompts() const { return m_prompts; }
    bool wasCancelled() const { return m_cancelled; }

private:
    QJsonObject initializeResult() const
    {
        QJsonObject capabilities{
            {"loadSession", m_loadSession},
            {"promptCapabilities",
             QJsonObject{{"image", m_images}, {"embeddedContext", m_embeddedContext}}},
            {"mcpCapabilities", QJsonObject{{"http", m_httpMcp}, {"sse", false}}}};
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

        for (int i = 0; i < m_toolCalls.size(); ++i) {
            QJsonObject update = m_toolCalls.at(i);
            update.insert(
                "sessionUpdate",
                i == 0 ? QStringLiteral("tool_call") : QStringLiteral("tool_call_update"));
            postUpdate(update);
        }

        for (const QJsonArray &entries : m_plans)
            postUpdate(QJsonObject{{"sessionUpdate", QStringLiteral("plan")}, {"entries", entries}});

        for (const QJsonArray &commands : m_availableCommandUpdates) {
            postUpdate(
                QJsonObject{
                    {"sessionUpdate", QStringLiteral("available_commands_update")},
                    {"availableCommands", commands}});
        }

        if (!m_contextGauge.isEmpty()) {
            QJsonObject update = m_contextGauge;
            update.insert("sessionUpdate", QStringLiteral("usage_update"));
            postUpdate(update);
        }

        m_activePrompt = request;

        if (!m_permissionToolCall.isEmpty()) {
            askPermission();
            if (!m_finishPromptWithoutWaiting)
                return;
        }

        if (m_hangOnPrompt)
            return;

        finishPrompt();
    }

    void askPermission()
    {
        post(
            QJsonObject{
                {"jsonrpc", QStringLiteral("2.0")},
                {"id", ++m_outgoingId},
                {"method", QStringLiteral("session/request_permission")},
                {"params",
                 QJsonObject{
                     {"sessionId", m_sessionId},
                     {"toolCall", m_permissionToolCall},
                     {"options", m_permissionOptions}}}});
    }

    void finishPrompt()
    {
        if (m_activePrompt.isEmpty())
            return;

        const QJsonObject request = m_activePrompt;
        m_activePrompt = QJsonObject();

        if (!m_promptError.isEmpty()) {
            replyError(request, -32603, m_promptError);
            return;
        }

        QJsonObject result{{"stopReason", QStringLiteral("end_turn")}};
        if (!m_promptUsage.isEmpty())
            result.insert("usage", m_promptUsage);
        reply(request, result);

        if (!m_suggestedTitle.isEmpty()) {
            postUpdate(
                QJsonObject{
                    {"sessionUpdate", QStringLiteral("session_info_update")},
                    {"title", m_suggestedTitle}});
        }
    }

    void postUpdate(const QJsonObject &update)
    {
        post(
            QJsonObject{
                {"jsonrpc", QStringLiteral("2.0")},
                {"method", QStringLiteral("session/update")},
                {"params", QJsonObject{{"sessionId", m_sessionId}, {"update", update}}}});
    }

    void notifyUpdate(const QString &kind, const QString &text)
    {
        postUpdate(
            QJsonObject{
                {"sessionUpdate", kind},
                {"content", QJsonObject{{"type", QStringLiteral("text")}, {"text", text}}}});
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
    bool m_embeddedContext = false;
    bool m_images = false;
    bool m_requireAuthentication = false;
    bool m_authenticated = false;
    bool m_cancelled = false;
    bool m_finishPromptWithoutWaiting = false;
    bool m_hangOnNewSession = false;
    bool m_httpMcp = false;
    QJsonArray m_offeredMcpServers;
    QString m_sessionId = QStringLiteral("fake-session");
    QString m_promptError;
    QString m_newSessionCwd;
    QString m_authMethodUsed;
    QJsonObject m_clientInfo;
    int m_outgoingId = 1000;
    QStringList m_chunks;
    QStringList m_thoughts;
    QStringList m_methods;
    QList<QJsonArray> m_prompts;
    QJsonObject m_activePrompt;
    QJsonObject m_permissionToolCall;
    QJsonArray m_permissionOptions;
    QList<QJsonObject> m_permissionOutcomes;
    QList<QJsonObject> m_toolCalls;
    QList<QJsonArray> m_plans;
    QList<QJsonArray> m_availableCommandUpdates;
    QJsonArray m_commandsBeforeSessionReply;
    QJsonObject m_promptUsage;
    QJsonObject m_contextGauge;
    QString m_suggestedTitle;
    QString m_loadSessionError;
    QString m_loadedSessionId;
};

} // namespace QodeAssist
