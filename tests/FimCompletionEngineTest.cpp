// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "FimCompletionEngineTest.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QTextDocument>

#include "CompletionTestSupport.hpp"
#include "FakeLlmProvider.hpp"
#include "completion/FimCompletionEngine.hpp"
#include "context/CompletionContextEnricher.hpp"
#include "context/ContextManager.hpp"
#include "context/TokenUtils.hpp"
#include "llmcore/ContextData.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"

namespace QodeAssist {

namespace {

class FakeCompletionEnricher : public Context::ICompletionEnricher
{
public:
    QString enrichmentFor(
        const Context::DocumentInfo &,
        const QString &prefix,
        int,
        int,
        int maxTokens) override
    {
        ++calls;
        lastPrefix = prefix;
        lastBudget = maxTokens;
        return enrichment;
    }

    int calls = 0;
    QString lastPrefix;
    int lastBudget = 0;
    QString enrichment = QStringLiteral("\n<semantic-context-marker>");
};

struct FimEngineFixture
{
    FimEngineFixture()
        : registry(&provider)
        , settings(new Settings::CodeCompletionSettings)
        , contextManager(nullptr)
        , engine(
              Settings::generalSettings(),
              *settings,
              registry,
              &promptProvider,
              reader,
              contextManager,
              performanceLogger,
              &enricher)
    {
        document.setPlainText("int main() {\n    return 0;\n}\n");
        reader.document = &document;

        settings->useSystemPrompt.setValue(true, Utils::BaseAspect::BeQuiet);
        settings->useUserMessageTemplateForCC.setValue(true, Utils::BaseAspect::BeQuiet);
        settings->useOpenFilesContext.setValue(false, Utils::BaseAspect::BeQuiet);
        settings->readFullFile.setValue(true, Utils::BaseAspect::BeQuiet);
        settings->modelOutputHandler.setValue(0, Utils::BaseAspect::BeQuiet);
        settings->completionMode.setValue(0, Utils::BaseAspect::BeQuiet);
    }

    FakeLlmProvider provider;
    FakeCompletionRegistry registry;
    FakeChatPromptProvider promptProvider;
    FakeCompletionDocumentReader reader;
    FakePerformanceLogger performanceLogger;
    QTextDocument document;
    QSharedPointer<Settings::CodeCompletionSettings> settings;
    Context::ContextManager contextManager;
    FakeCompletionEnricher enricher;
    FimCompletionEngine engine;
};

} // namespace

void FimCompletionEngineTest::testFimEngineCompletesThroughTheProvider()
{
    FimEngineFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(7, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    auto *client = fixture.provider.fakeClient();
    QCOMPARE(client->requestCounter, 1);
    QVERIFY(client->lastPayload.contains("model"));
    QCOMPARE(client->lastPayload.value("stream").toBool(), true);

    QVERIFY(fixture.provider.lastContext.history.has_value());
    QCOMPARE(fixture.provider.lastContext.history->size(), 1);
    QCOMPARE(fixture.provider.lastContext.history->first().role, QStringLiteral("user"));
    QVERIFY(fixture.provider.lastContext.history->first().content.contains("int main()"));
    QVERIFY(fixture.provider.lastContext.systemPrompt.has_value());

    client->completeRequest(client->lastRequestId, QStringLiteral("done();"));

    QCOMPARE(failed.count(), 0);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(7));
    QCOMPARE(ready.first().at(1).toString(), QStringLiteral("done();"));
}

void FimCompletionEngineTest::testFimEngineCancelIsPerRequest()
{
    FimEngineFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(1, {QStringLiteral("/path/to/a.cpp"), 0, 0});
    const auto firstLlmId = fixture.provider.fakeClient()->lastRequestId;
    fixture.engine.request(2, {QStringLiteral("/path/to/b.cpp"), 0, 0});
    const auto secondLlmId = fixture.provider.fakeClient()->lastRequestId;
    QVERIFY(firstLlmId != secondLlmId);

    fixture.engine.cancel(1);

    auto *client = fixture.provider.fakeClient();
    client->completeRequest(firstLlmId, QStringLiteral("stale"));
    QCOMPARE(ready.count(), 0);

    client->completeRequest(secondLlmId, QStringLiteral("fresh"));
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(0).toULongLong(), quint64(2));
    QCOMPARE(ready.first().at(1).toString(), QStringLiteral("fresh"));
    QCOMPARE(failed.count(), 0);
}

void FimCompletionEngineTest::testFimEngineStripsCodeFences()
{
    FimEngineFixture fixture;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);

    fixture.engine.request(3, {QStringLiteral("/path/to/file.cpp"), 1, 4});
    auto *client = fixture.provider.fakeClient();
    client->completeRequest(client->lastRequestId, QStringLiteral("```cpp\nint x = 1;\n```\n"));

    QCOMPARE(ready.count(), 1);
    const QString text = ready.first().at(1).toString();
    QVERIFY(!text.contains(QStringLiteral("```")));
    QVERIFY(text.contains(QStringLiteral("int x = 1;")));
}

void FimCompletionEngineTest::testFimEngineFailsWithoutDocument()
{
    FimEngineFixture fixture;
    fixture.reader.document = nullptr;

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(4, {QStringLiteral("/path/to/missing.cpp"), 0, 0});

    QCOMPARE(fixture.provider.fakeClient()->requestCounter, 0);
    QCOMPARE(ready.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toULongLong(), quint64(4));
    QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("Document is not available")));
}

void FimCompletionEngineTest::testFimEngineEnrichmentFollowsCompletionMode()
{
    FimEngineFixture fixture;

    fixture.engine.request(10, {QStringLiteral("/path/to/file.cpp"), 1, 4});
    QCOMPARE(fixture.enricher.calls, 0);
    QVERIFY(fixture.provider.lastContext.systemPrompt.has_value());
    QVERIFY(!fixture.provider.lastContext.systemPrompt->contains(
        QStringLiteral("<semantic-context-marker>")));

    fixture.settings->completionMode.setValue(1, Utils::BaseAspect::BeQuiet);
    QCOMPARE(fixture.settings->completionMode.stringValue(), QStringLiteral("FIM + context"));

    fixture.engine.request(11, {QStringLiteral("/path/to/file.cpp"), 1, 4});
    QCOMPARE(fixture.enricher.calls, 1);
    QVERIFY(fixture.enricher.lastBudget > 0);
    QVERIFY(fixture.enricher.lastPrefix.contains(QStringLiteral("int main()")));
    QVERIFY(fixture.provider.lastContext.systemPrompt->contains(
        QStringLiteral("<semantic-context-marker>")));
}

void FimCompletionEngineTest::testFimEngineEnrichmentSkipsFimTemplates()
{
    FimEngineFixture fixture;
    fixture.settings->completionMode.setValue(1, Utils::BaseAspect::BeQuiet);
    fixture.promptProvider.useFimTemplate = true;

    fixture.engine.request(12, {QStringLiteral("/path/to/file.cpp"), 1, 4});

    QCOMPARE(fixture.enricher.calls, 0);
    QVERIFY(fixture.provider.lastContext.systemPrompt.has_value());
    QVERIFY(!fixture.provider.lastContext.systemPrompt->contains(
        QStringLiteral("<semantic-context-marker>")));
}

void FimCompletionEngineTest::testIdentifiersNearCursorOrdersAndFilters()
{
    const QString prefix = QStringLiteral(
        "class Widget;\nvoid setupWidget(Widget *widget) {\n    widget->applyTheme(theme);\n");

    const QStringList identifiers = Context::identifiersNearCursor(prefix, 3);

    QCOMPARE(identifiers.size(), 3);
    QCOMPARE(identifiers.at(0), QStringLiteral("theme"));
    QCOMPARE(identifiers.at(1), QStringLiteral("applyTheme"));
    QCOMPARE(identifiers.at(2), QStringLiteral("widget"));
    QVERIFY(!identifiers.contains(QStringLiteral("void")));
    QVERIFY(!identifiers.contains(QStringLiteral("class")));

    const QStringList unlimited = Context::identifiersNearCursor(prefix, 10);
    QVERIFY(unlimited.contains(QStringLiteral("Widget")));
    QCOMPARE(unlimited.count(QStringLiteral("widget")), 1);
}

void FimCompletionEngineTest::testClampSectionsToTokenBudget()
{
    const QString small = QStringLiteral("one two");
    const QString large = QString(QStringLiteral("word ")).repeated(400);
    const QString tail = QStringLiteral("three four");

    const int budget = Context::TokenUtils::estimateTokens(small)
                       + Context::TokenUtils::estimateTokens(tail) + 1;

    const QString clamped = Context::clampSectionsToTokenBudget({small, large, tail}, budget);

    QVERIFY(clamped.contains(small));
    QVERIFY(clamped.contains(tail));
    QVERIFY(!clamped.contains(QStringLiteral("word word")));

    QCOMPARE(Context::clampSectionsToTokenBudget({large}, 1), QString());
    QCOMPARE(Context::clampSectionsToTokenBudget({}, 100), QString());
}

void FimCompletionEngineTest::testFimEngineFailsWithoutProvider()
{
    FimEngineFixture fixture;
    fixture.registry.setProvider(nullptr);

    QSignalSpy ready(&fixture.engine, &CompletionEngine::completionReady);
    QSignalSpy failed(&fixture.engine, &CompletionEngine::completionFailed);

    fixture.engine.request(5, {QStringLiteral("/path/to/file.cpp"), 0, 0});

    QCOMPARE(fixture.provider.fakeClient()->requestCounter, 0);
    QCOMPARE(ready.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(0).toULongLong(), quint64(5));
    QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("No provider found")));
}

} // namespace QodeAssist
