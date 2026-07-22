// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgenticCompletionEngineTest.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QTextDocument>

#include <LLMQore/ToolsManager.hpp>

#include "CompletionTestSupport.hpp"
#include "FakeLlmProvider.hpp"
#include "completion/AgenticCompletionEngine.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "tools/ProposeCompletionTool.hpp"

namespace QodeAssist {

namespace {

struct AgenticEngineFixture
{
    AgenticEngineFixture()
        : registry(&provider)
        , settings(new Settings::CodeCompletionSettings)
        , engine(
              Settings::generalSettings(),
              *settings,
              registry,
              &promptProvider,
              reader,
              performanceLogger)
    {
        document.setPlainText("int main() {\n    return 0;\n}\n");
        reader.document = &document;

        settings->useUserMessageTemplateForCC.setValue(true, Utils::BaseAspect::BeQuiet);
        settings->readFullFile.setValue(true, Utils::BaseAspect::BeQuiet);
    }

    Tools::ProposeCompletionTool *registeredTool() const
    {
        return qobject_cast<Tools::ProposeCompletionTool *>(
            provider.fakeClient()->tools()->tool(Tools::ProposeCompletionTool::toolId()));
    }

    FakeLlmProvider provider;
    FakeCompletionRegistry registry;
    FakeChatPromptProvider promptProvider;
    FakeCompletionDocumentReader reader;
    FakePerformanceLogger performanceLogger;
    QTextDocument document;
    QSharedPointer<Settings::CodeCompletionSettings> settings;
    AgenticCompletionEngine engine;
};

} // anonymous namespace

void AgenticCompletionEngineTest::testProposeToolValidatesArguments()
{
    Tools::ProposeCompletionTool tool;
    QSignalSpy proposed(&tool, &Tools::ProposeCompletionTool::completionProposed);

    const auto missing = tool.executeAsync(QJsonObject{}).result();
    QVERIFY(missing.isError);
    QCOMPARE(proposed.count(), 0);

    const auto valid = tool.executeAsync(
                               QJsonObject{
                                   {"file", "/path/to/file.cpp"},
                                   {"line", 1},
                                   {"column", 4},
                                   {"text", "return 0;"}})
                           .result();
    QVERIFY(!valid.isError);
    QCOMPARE(proposed.count(), 1);
    QCOMPARE(proposed.first().at(0).toString(), QStringLiteral("/path/to/file.cpp"));
    QCOMPARE(proposed.first().at(3).toString(), QStringLiteral("return 0;"));
}

void AgenticCompletionEngineTest::testAgenticEngineRendersProposalFromTool()
{
    AgenticEngineFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(21, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    QCOMPARE(fixture.provider.fakeClient()->requestCounter, 1);
    QVERIFY(fixture.provider.lastToolsEnabled);
    QVERIFY(fixture.provider.lastContext.systemPrompt.has_value());
    QVERIFY(fixture.provider.lastContext.systemPrompt->contains(
        QStringLiteral("propose_completion")));
    QVERIFY(fixture.registeredTool() != nullptr);

    const auto result = fixture.registeredTool()
                            ->executeAsync(
                                QJsonObject{
                                    {"file", "/path/to/file.cpp"}, {"text", "return 0;"}})
                            .result();
    QVERIFY(!result.isError);

    QCOMPARE(failed.count(), 0);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(21));
    QCOMPARE(ready.first().at(1).toString(), QStringLiteral("return 0;"));

    fixture.provider.fakeClient()->completeRequest(
        fixture.provider.fakeClient()->lastRequestId, QStringLiteral("done"));
    QCOMPARE(failed.count(), 0);
    QCOMPARE(ready.count(), 1);
}

void AgenticCompletionEngineTest::testAgenticEngineRejectsProviderWithoutTools()
{
    AgenticEngineFixture fixture;
    fixture.provider.fakeCapabilities = {};

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(22, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    QCOMPARE(fixture.provider.fakeClient()->requestCounter, 0);
    QCOMPARE(ready.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toULongLong(), quint64(22));
    QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("tool calling")));
}

void AgenticCompletionEngineTest::testAgenticEngineRejectsFimTemplate()
{
    AgenticEngineFixture fixture;
    fixture.promptProvider.useFimTemplate = true;

    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(23, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    QCOMPARE(fixture.provider.fakeClient()->requestCounter, 0);
    QCOMPARE(failed.count(), 1);
    QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("chat-type")));
}

void AgenticCompletionEngineTest::testAgenticEngineFailsWhenModelProposesNothing()
{
    AgenticEngineFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(24, {QStringLiteral("/path/to/file.cpp"), 1, 4});
    fixture.provider.fakeClient()->completeRequest(
        fixture.provider.fakeClient()->lastRequestId, QStringLiteral("here is some prose"));

    QCOMPARE(ready.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toULongLong(), quint64(24));
    QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("without proposing")));
}

void AgenticCompletionEngineTest::testAgenticEngineRoutesLoneProposalRegardlessOfFile()
{
    AgenticEngineFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);

    fixture.engine.request(25, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    const auto result = fixture.registeredTool()
                            ->executeAsync(
                                QJsonObject{
                                    {"file", "/path/to/other.cpp"}, {"text", "adopted();"}})
                            .result();
    QVERIFY(!result.isError);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(25));
    QCOMPARE(ready.first().at(1).toString(), QStringLiteral("adopted();"));

    const auto idle = fixture.registeredTool()
                          ->executeAsync(
                              QJsonObject{{"file", "/path/to/file.cpp"}, {"text", "late();"}})
                          .result();
    QVERIFY(!idle.isError);
    QCOMPARE(ready.count(), 1);
}

} // namespace QodeAssist
