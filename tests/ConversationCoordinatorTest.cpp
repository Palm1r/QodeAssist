// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ConversationCoordinatorTest.hpp"

#include <QSignalSpy>
#include <QTest>

#include "ChatView/ChatConfigurationController.hpp"
#include "ChatView/ConversationCoordinator.hpp"
#include "ChatView/ConversationPorts.hpp"
#include "acp/AgentBinding.hpp"
#include "acp/AgentDefinition.hpp"

namespace QodeAssist {

namespace {

Acp::AgentDefinition launchableAgent(const QString &id)
{
    Acp::AgentDefinition agent;
    agent.id = id;
    agent.name = id;
    agent.distribution.kind = Acp::AgentDistributionKind::Command;
    agent.distribution.command = "/bin/true";
    return agent;
}

class FakeConversationOps : public Chat::IConversationPort
{
public:
    QString boundAgentId() const override { return boundId; }
    bool conversationStarted() const override { return started; }
    bool transcriptEmpty() const override { return transcriptIsEmpty; }
    Acp::AgentBinding agentBinding() const override { return liveBinding; }

    void bindAgent(const Acp::AgentDefinition &agent) override
    {
        boundId = agent.id;
        calls.append("bindAgent:" + agent.id);
    }

    void bindLlm() override
    {
        boundId.clear();
        calls.append("bindLlm");
    }

    void clearConversation() override
    {
        started = false;
        calls.append("clear");
    }

    void resumeAgentSession(const QString &sessionId) override
    {
        calls.append("resume:" + sessionId);
    }

    void startFreshAgentSession() override { calls.append("fresh"); }

    void startFreshAgentSession(const QString &handoverSummary) override
    {
        calls.append("freshWithSummary:" + handoverSummary);
    }

    void releaseAgentSession() override { calls.append("release"); }

    QString boundId;
    bool started = false;
    bool transcriptIsEmpty = true;
    Acp::AgentBinding liveBinding;
    QStringList calls;
};

class FakeAgentCatalog : public Chat::IAgentCatalogPort
{
public:
    std::optional<Acp::AgentDefinition> agentById(const QString &agentId) const override
    {
        const auto it = agents.constFind(agentId);
        if (it == agents.constEnd())
            return std::nullopt;
        return *it;
    }

    QHash<QString, Acp::AgentDefinition> agents;
};

class FakeCompressionPort : public Chat::ICompressionPort
{
public:
    QString compressionConfigurationIssue() const override { return configurationIssue; }
    bool isCompressionRunning() const override { return running; }
    void startTranscriptSummary() override { ++summaryStarts; }
    void startCompression() override { ++compressionStarts; }

    QString configurationIssue;
    bool running = false;
    int summaryStarts = 0;
    int compressionStarts = 0;
};

class FakeSendPort : public Chat::ISendPort
{
public:
    bool autoCompressEnabled() const override { return autoCompress; }
    int autoCompressThreshold() const override { return threshold; }
    int estimatedNextTokens() const override { return tokens; }

    bool prepareChatFileForCompression(const QString &, const QStringList &) override
    {
        return chatFileAvailable;
    }

    void dispatch(const QString &message, const QStringList &) override
    {
        dispatched.append(message);
    }

    bool autoCompress = false;
    int threshold = 1000;
    int tokens = 0;
    bool chatFileAvailable = true;
    QStringList dispatched;
};

class CoordinatorFixture
{
public:
    CoordinatorFixture()
        : coordinator({&ops, &catalog, &compression, &send})
    {
        QObject::connect(
            &coordinator,
            &Chat::ConversationCoordinator::errorSurfaced,
            &coordinator,
            [this](const QString &message) { errors.append(message); });
        QObject::connect(
            &coordinator,
            &Chat::ConversationCoordinator::switchConfirmationNeeded,
            &coordinator,
            [this](const QString &targetName) { confirmations.append(targetName); });
    }

    FakeConversationOps ops;
    FakeAgentCatalog catalog;
    FakeCompressionPort compression;
    FakeSendPort send;
    QStringList errors;
    QStringList confirmations;
    Chat::ConversationCoordinator coordinator;
};

} // namespace

void ConversationCoordinatorTest::testCoordinatorSwitchConfirmAndCancelRoundTrip()
{
    CoordinatorFixture fixture;

    fixture.coordinator.chooseAgent(launchableAgent("agent-a"));
    QVERIFY(!fixture.coordinator.switchPending());
    QCOMPARE(fixture.ops.calls, QStringList{"bindAgent:agent-a"});

    fixture.ops.started = true;
    fixture.ops.calls.clear();

    fixture.coordinator.chooseLlm();
    QVERIFY(fixture.coordinator.switchPending());
    QCOMPARE(fixture.confirmations.size(), 1);
    QVERIFY(fixture.ops.calls.isEmpty());

    fixture.coordinator.cancelSwitch();
    QVERIFY(!fixture.coordinator.switchPending());
    QVERIFY(fixture.ops.calls.isEmpty());
    QCOMPARE(fixture.ops.boundId, QString("agent-a"));

    fixture.coordinator.chooseAgent(launchableAgent("agent-b"));
    QVERIFY(fixture.coordinator.switchPending());
    QCOMPARE(fixture.confirmations.size(), 2);

    fixture.coordinator.confirmSwitch();
    QVERIFY(!fixture.coordinator.switchPending());
    QCOMPARE(fixture.ops.calls, (QStringList{"clear", "bindAgent:agent-b"}));
    QVERIFY(!fixture.ops.started);

    fixture.coordinator.confirmSwitch();
    QCOMPARE(fixture.ops.calls, (QStringList{"clear", "bindAgent:agent-b"}));
}

void ConversationCoordinatorTest::testCoordinatorQuarantinesAndRestoresAgentBindings()
{
    CoordinatorFixture fixture;
    const Acp::AgentBinding binding{"ghost-agent", "sess-1"};

    fixture.coordinator.restoreAgentBinding(binding);

    QVERIFY(fixture.coordinator.readOnly());
    QVERIFY(!fixture.coordinator.canStartFreshSession());
    QCOMPARE(fixture.coordinator.bindingForSave(), binding);
    QCOMPARE(fixture.ops.calls, (QStringList{"release", "bindLlm"}));

    fixture.ops.calls.clear();
    fixture.catalog.agents.insert("ghost-agent", launchableAgent("ghost-agent"));

    fixture.coordinator.restoreAgentBinding(binding);

    QVERIFY(!fixture.coordinator.readOnly());
    QCOMPARE(fixture.ops.calls, (QStringList{"release", "bindAgent:ghost-agent", "resume:sess-1"}));
    fixture.ops.liveBinding = Acp::AgentBinding{"ghost-agent", "sess-1"};
    QCOMPARE(fixture.coordinator.bindingForSave(), fixture.ops.liveBinding);

    fixture.ops.calls.clear();
    fixture.ops.started = true;

    fixture.coordinator.restoreAgentBinding(Acp::AgentBinding{"ghost-agent", QString()});

    QVERIFY(fixture.coordinator.readOnly());
    QVERIFY(fixture.coordinator.canStartFreshSession());
    QCOMPARE(fixture.ops.calls, (QStringList{"release", "bindAgent:ghost-agent"}));

    fixture.coordinator.startFreshSession();
    QVERIFY(!fixture.coordinator.readOnly());
    QVERIFY(fixture.ops.calls.contains("fresh"));
}

void ConversationCoordinatorTest::testCoordinatorRefusesIntentsWhileReadOnly()
{
    CoordinatorFixture fixture;
    fixture.coordinator.agentSessionUnavailable("gone");
    QVERIFY(fixture.coordinator.readOnly());

    fixture.coordinator.requestSend("hi", {});
    QCOMPARE(fixture.send.dispatched.size(), 0);
    QCOMPARE(fixture.errors.size(), 1);

    QVERIFY(fixture.coordinator.refuseWhileReadOnly());
    QCOMPARE(fixture.errors.size(), 2);

    fixture.coordinator.compressionSettled();
    QCOMPARE(fixture.send.dispatched.size(), 0);

    fixture.coordinator.startFreshSession();
    QVERIFY(!fixture.coordinator.readOnly());
    QVERIFY(!fixture.coordinator.refuseWhileReadOnly());

    fixture.coordinator.requestSend("hi again", {});
    QCOMPARE(fixture.send.dispatched, QStringList{"hi again"});
}

void ConversationCoordinatorTest::testCoordinatorDefersTheSendForAutoCompress()
{
    CoordinatorFixture fixture;
    fixture.send.autoCompress = true;
    fixture.send.threshold = 100;
    fixture.send.tokens = 150;

    fixture.coordinator.requestSend("big message", {});
    QCOMPARE(fixture.send.dispatched.size(), 0);
    QCOMPARE(fixture.compression.compressionStarts, 1);
    QVERIFY(fixture.coordinator.hasDeferredSend());

    fixture.coordinator.requestSend("while compressing", {});
    QCOMPARE(fixture.send.dispatched, QStringList{"while compressing"});
    QCOMPARE(fixture.compression.compressionStarts, 1);

    fixture.coordinator.compressionSettled();
    QCOMPARE(fixture.send.dispatched, (QStringList{"while compressing", "big message"}));
    QVERIFY(!fixture.coordinator.hasDeferredSend());

    fixture.send.tokens = 50;
    fixture.coordinator.requestSend("small", {});
    QCOMPARE(fixture.send.dispatched.size(), 3);
    QCOMPARE(fixture.compression.compressionStarts, 1);

    fixture.send.tokens = 150;
    fixture.ops.boundId = "agent-a";
    fixture.coordinator.requestSend("agent bound", {});
    QCOMPARE(fixture.send.dispatched.size(), 4);
    QCOMPARE(fixture.compression.compressionStarts, 1);

    fixture.ops.boundId.clear();
    fixture.send.chatFileAvailable = false;
    fixture.coordinator.requestSend("no chat file", {});
    QCOMPARE(fixture.send.dispatched.size(), 5);
    QCOMPARE(fixture.compression.compressionStarts, 1);
}

void ConversationCoordinatorTest::testCoordinatorGatesTheSummaryHandover()
{
    CoordinatorFixture fixture;
    fixture.coordinator.agentSessionUnavailable("gone");
    fixture.ops.transcriptIsEmpty = false;

    QVERIFY(fixture.coordinator.canHandOverSummary());
    fixture.coordinator.handOverSummary();
    QCOMPARE(fixture.compression.summaryStarts, 1);

    fixture.coordinator.summaryProduced("the summary");
    QVERIFY(fixture.ops.calls.contains("freshWithSummary:the summary"));
    QVERIFY(!fixture.coordinator.readOnly());

    fixture.coordinator.agentSessionUnavailable("gone again");
    fixture.compression.configurationIssue = "no chat configuration is assigned";
    QVERIFY(!fixture.coordinator.canHandOverSummary());
    QVERIFY(fixture.coordinator.summaryHandoverTooltip().contains("cannot be produced"));
    fixture.coordinator.handOverSummary();
    QCOMPARE(fixture.compression.summaryStarts, 1);

    fixture.compression.configurationIssue.clear();
    fixture.ops.transcriptIsEmpty = true;
    QVERIFY(!fixture.coordinator.canHandOverSummary());

    fixture.ops.transcriptIsEmpty = false;
    fixture.compression.running = true;
    QVERIFY(fixture.coordinator.canHandOverSummary());
    fixture.coordinator.handOverSummary();
    QCOMPARE(fixture.compression.summaryStarts, 1);
    QVERIFY(!fixture.errors.isEmpty());
}

void ConversationCoordinatorTest::testCoordinatorShrinkContextRoutesByBackendKind()
{
    CoordinatorFixture fixture;
    fixture.ops.transcriptIsEmpty = false;

    fixture.coordinator.shrinkContext();
    QCOMPARE(fixture.compression.compressionStarts, 1);
    QCOMPARE(fixture.compression.summaryStarts, 0);

    fixture.compression.configurationIssue = "no chat configuration is assigned";
    QVERIFY(!fixture.coordinator.canShrinkContext());
    fixture.coordinator.shrinkContext();
    QCOMPARE(fixture.compression.compressionStarts, 1);
    QCOMPARE(fixture.errors.size(), 1);
    QVERIFY(fixture.coordinator.shrinkContextTooltip().contains("Unavailable"));
    fixture.compression.configurationIssue.clear();

    fixture.ops.boundId = "agent-a";
    QVERIFY(fixture.coordinator.canShrinkContext());
    fixture.coordinator.shrinkContext();
    QCOMPARE(fixture.compression.compressionStarts, 1);
    QCOMPARE(fixture.compression.summaryStarts, 1);

    fixture.ops.transcriptIsEmpty = true;
    QVERIFY(!fixture.coordinator.canShrinkContext());
    fixture.coordinator.shrinkContext();
    QCOMPARE(fixture.compression.summaryStarts, 1);
    QVERIFY(fixture.coordinator.shrinkContextTooltip().contains("nothing to summarise"));

    fixture.ops.transcriptIsEmpty = false;
    fixture.compression.configurationIssue = "no chat configuration is assigned";
    QVERIFY(!fixture.coordinator.canShrinkContext());
    fixture.coordinator.shrinkContext();
    QCOMPARE(fixture.compression.summaryStarts, 1);
    fixture.compression.configurationIssue.clear();

    fixture.compression.running = true;
    const int errorsBeforeBusy = fixture.errors.size();
    fixture.coordinator.shrinkContext();
    QCOMPARE(fixture.compression.summaryStarts, 1);
    QCOMPARE(fixture.errors.size(), errorsBeforeBusy + 1);

    const int dispatchedBeforeBusySend = fixture.send.dispatched.size();
    fixture.coordinator.requestSend("while summarising", {});
    QCOMPARE(fixture.send.dispatched.size(), dispatchedBeforeBusySend);
    QCOMPARE(fixture.errors.size(), errorsBeforeBusy + 2);
    fixture.compression.running = false;

    fixture.coordinator.agentSessionUnavailable("gone");
    QVERIFY(!fixture.coordinator.canShrinkContext());
    const int errorsBeforeReadOnly = fixture.errors.size();
    fixture.coordinator.shrinkContext();
    QCOMPARE(fixture.errors.size(), errorsBeforeReadOnly + 1);
    QCOMPARE(fixture.compression.summaryStarts, 1);
    QCOMPARE(fixture.compression.compressionStarts, 1);
}

} // namespace QodeAssist
