// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>

#include <optional>

#include "ChatView/ChatModel.hpp"
#include "session/BlockCodec.hpp"
#include "session/ChatBackend.hpp"
#include "session/ContentBlock.hpp"
#include "session/ConversationHistory.hpp"
#include "session/HistoryProjection.hpp"
#include "session/PermissionRequest.hpp"
#include "session/Session.hpp"
#include "session/SessionEvent.hpp"
#include "session/TurnRequest.hpp"

namespace QodeAssist {

inline Session::Message userTurn()
{
    Session::Message message;
    message.role = Session::MessageRole::User;
    message.id = "u1";
    message.blocks
        = {Session::TextBlock{"explain this"},
           Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"},
           Session::ImageBlock{"shot.png", "shot_cd34.png", "image/png"}};
    return message;
}

inline Session::Message assistantTurn()
{
    Session::Message message;
    message.role = Session::MessageRole::Assistant;
    message.id = "r1";
    message.blocks
        = {Session::ThinkingBlock{"weighing options", "sig-1", false},
           Session::ToolCallBlock{
               "t1", "read_file", QJsonObject{{"path", "main.cpp"}}, "int main() {}"},
           Session::TextBlock{"here is the answer"}};
    message.usage = Session::Usage{120, 40, 80, 12};
    return message;
}

inline Session::ConversationHistory sampleHistory()
{
    Session::Message assistant = assistantTurn();
    assistant.blocks.append(
        Session::FileEditBlock{"e1", "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\"}"});

    Session::ConversationHistory history;
    history.append(userTurn());
    history.append(assistant);
    return history;
}

inline Session::ConversationHistory historyWithoutFileEdits()
{
    Session::ConversationHistory history;
    history.append(userTurn());
    history.append(assistantTurn());
    return history;
}

class FakeChatBackend : public Session::ChatBackend
{
public:
    void sendTurn(const Session::TurnRequest &request) override
    {
        ++m_turnCount;
        m_lastUserBlocks = request.userBlocks;
        m_historySizeAtSend = request.history ? request.history->size() : -1;
        m_lastContext = request.context;
    }

    void cancel() override { ++m_cancelCount; }

    bool respondPermission(const QString &requestId, const QString &optionId) override
    {
        m_permissionAnswers.append({requestId, optionId});
        if (!m_acceptPermissionAnswers)
            return false;
        if (m_autoResolvePermissions)
            emit sessionEvent(Session::PermissionResolved{m_turnId, requestId, optionId, false});
        return true;
    }

    void script(const QList<Session::SessionEvent> &events)
    {
        for (const Session::SessionEvent &scripted : events) {
            if (const auto *started = std::get_if<Session::TurnStarted>(&scripted))
                m_turnId = started->turnId;
            emit sessionEvent(scripted);
        }
    }

    void setAutoResolvePermissions(bool automatic) { m_autoResolvePermissions = automatic; }
    void setAcceptPermissionAnswers(bool accept) { m_acceptPermissionAnswers = accept; }

    QList<QPair<QString, QString>> permissionAnswers() const { return m_permissionAnswers; }

    int turnCount() const { return m_turnCount; }
    int cancelCount() const { return m_cancelCount; }
    qsizetype historySizeAtSend() const { return m_historySizeAtSend; }
    const QList<Session::ContentBlock> &lastUserBlocks() const { return m_lastUserBlocks; }
    const std::optional<Session::TurnContext> &lastContext() const { return m_lastContext; }

private:
    int m_turnCount = 0;
    int m_cancelCount = 0;
    bool m_autoResolvePermissions = true;
    bool m_acceptPermissionAnswers = true;
    qsizetype m_historySizeAtSend = -1;
    QString m_turnId;
    QList<QPair<QString, QString>> m_permissionAnswers;
    QList<Session::ContentBlock> m_lastUserBlocks;
    std::optional<Session::TurnContext> m_lastContext;
};

inline QList<Session::PermissionOption> editPermissionOptions()
{
    return {
        Session::PermissionOption{"opt-once", "Yes", "allow_once"},
        Session::PermissionOption{"opt-always", "Yes, and don't ask again", "allow_always"},
        Session::PermissionOption{"opt-no", "No", "reject_once"}};
}

inline Session::PermissionRequested editPermissionRequest(
    const QString &turnId, const QString &requestId, const QString &toolKind = QStringLiteral("edit"))
{
    return Session::PermissionRequested{
        .turnId = turnId,
        .requestId = requestId,
        .toolCallId = "tool-" + requestId,
        .title = "Edit main.cpp",
        .toolKind = toolKind,
        .options = editPermissionOptions()};
}

inline Session::PermissionBlock permissionRowAt(const Session::Session &session, int index)
{
    return Session::decodePermissionBlock(session.rows().at(index).content)
        .value_or(Session::PermissionBlock{});
}

inline Session::ToolCallUpdated llmToolStarted(
    const QString &turnId,
    const QString &toolId,
    const QString &name,
    const QJsonObject &arguments = {},
    bool dropPrecedingText = false)
{
    return Session::ToolCallUpdated{
        .turnId = turnId,
        .toolId = toolId,
        .name = name,
        .status = "in_progress",
        .arguments = arguments,
        .dropPrecedingText = dropPrecedingText};
}

inline Session::ToolCallUpdated llmToolCompleted(
    const QString &turnId, const QString &toolId, const QString &name, const QString &result)
{
    return Session::ToolCallUpdated{
        .turnId = turnId,
        .toolId = toolId,
        .name = name,
        .status = "completed",
        .result = result};
}

inline QList<Session::SessionEvent> scriptedTurn(const QString &turnId)
{
    return {
        Session::TurnStarted{turnId},
        Session::ThinkingReceived{turnId, "weighing options", "sig-1", false},
        Session::TextDelta{turnId, "let me "},
        Session::TextDelta{turnId, "check"},
        llmToolStarted(turnId, "t1", "read_file", QJsonObject{{"path", "main.cpp"}}),
        llmToolCompleted(turnId, "t1", "read_file", "int main() {}"),
        Session::TextDelta{turnId, "here is the answer"},
        Session::UsageReported{turnId, Session::Usage{120, 40, 80, 12}},
        Session::TurnCompleted{turnId}};
}

inline void attachModelToSession(Session::Session &session, Chat::ChatModel &model)
{
    QObject::connect(
        &session, &Session::Session::rowsReset, &model, &Chat::ChatModel::resetMessages);
    QObject::connect(
        &session, &Session::Session::rowsAppended, &model, &Chat::ChatModel::appendMessages);
    QObject::connect(
        &session, &Session::Session::rowUpdated, &model, &Chat::ChatModel::updateMessage);
    QObject::connect(
        &session, &Session::Session::rowsRemoved, &model, &Chat::ChatModel::removeMessages);
    model.resetMessages(session.rows());
}

} // namespace QodeAssist
