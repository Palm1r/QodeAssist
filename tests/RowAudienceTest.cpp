// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "RowAudienceTest.hpp"

#include <QTest>

#include "session/AgentPlan.hpp"
#include "session/ContentBlock.hpp"
#include "session/HistoryProjection.hpp"
#include "session/PermissionRequest.hpp"
#include "session/Session.hpp"

namespace QodeAssist {

namespace {

Session::ConversationHistory historyWithEveryRowKind()
{
    Session::Message system;
    system.role = Session::MessageRole::System;
    system.id = "s1";
    system.blocks = {Session::TextBlock{"system"}};

    Session::Message user;
    user.role = Session::MessageRole::User;
    user.id = "u1";
    user.blocks = {Session::TextBlock{"question"}};

    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "a1";
    assistant.blocks
        = {Session::TextBlock{"answer"},
           Session::ThinkingBlock{"thinking", {}, false},
           Session::ToolCallBlock{"call-1", "read_file", {}, "int main() {}", {}, {}, {}, false},
           Session::ToolCallBlock{
               "call-2", "Edit main.cpp", {}, "done", "edit", "completed", {}, true},
           Session::FileEditBlock{"edit-1", "payload"},
           Session::PermissionBlock{"p1", "tool-p1", "Edit main.cpp", "edit", {}},
           Session::PlanBlock{{Session::PlanEntry{"Step", "high", "pending"}}}};

    Session::ConversationHistory history;
    history.append(system);
    history.append(user);
    history.append(assistant);
    return history;
}

QList<Session::RowKind> projectedKinds()
{
    QList<Session::RowKind> kinds;
    for (const Session::MessageRow &row : Session::projectToRows(historyWithEveryRowKind()))
        kinds.append(row.kind);
    return kinds;
}

QList<Session::RowKind> survivingKinds(Session::RowAudience audience)
{
    QList<Session::RowKind> kept;
    for (const Session::MessageRow &row : Session::projectToRows(historyWithEveryRowKind())) {
        if (Session::rowTreatmentFor(audience, row.kind) != Session::RowTreatment::Omit)
            kept.append(row.kind);
    }
    return kept;
}

} // namespace

void RowAudienceTest::testEveryRowKindProjectsInOrder()
{
    QCOMPARE(
        projectedKinds(),
        (QList<Session::RowKind>{
            Session::RowKind::System,
            Session::RowKind::User,
            Session::RowKind::Assistant,
            Session::RowKind::Thinking,
            Session::RowKind::Tool,
            Session::RowKind::AgentTool,
            Session::RowKind::FileEdit,
            Session::RowKind::Permission,
            Session::RowKind::Plan}));
}

void RowAudienceTest::testPromptAudienceKeepsToolsAndThinking()
{
    QCOMPARE(
        survivingKinds(Session::RowAudience::Prompt),
        (QList<Session::RowKind>{
            Session::RowKind::System,
            Session::RowKind::User,
            Session::RowKind::Assistant,
            Session::RowKind::Thinking,
            Session::RowKind::Tool}));

    QCOMPARE(
        Session::rowTreatmentFor(Session::RowAudience::Prompt, Session::RowKind::User),
        Session::RowTreatment::UserText);
    QCOMPARE(
        Session::rowTreatmentFor(Session::RowAudience::Prompt, Session::RowKind::System),
        Session::RowTreatment::AssistantText);
    QCOMPARE(
        Session::rowTreatmentFor(Session::RowAudience::Prompt, Session::RowKind::Thinking),
        Session::RowTreatment::AssistantThinking);
    QCOMPARE(
        Session::rowTreatmentFor(Session::RowAudience::Prompt, Session::RowKind::Tool),
        Session::RowTreatment::ToolExchange);
}

void RowAudienceTest::testCompressionAudienceDropsToolsAndThinking()
{
    QCOMPARE(
        survivingKinds(Session::RowAudience::Compression),
        (QList<Session::RowKind>{
            Session::RowKind::System, Session::RowKind::User, Session::RowKind::Assistant}));
}

void RowAudienceTest::testTokenCountAudienceCountsToolsAndThinking()
{
    QCOMPARE(
        survivingKinds(Session::RowAudience::TokenCount),
        (QList<Session::RowKind>{
            Session::RowKind::System,
            Session::RowKind::User,
            Session::RowKind::Assistant,
            Session::RowKind::Thinking,
            Session::RowKind::Tool}));
}

} // namespace QodeAssist
