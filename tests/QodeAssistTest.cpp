// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "QodeAssistTest.hpp"

#include "completion/CodeHandler.hpp"
#include "completion/LLMSuggestion.hpp"
#include "llmcore/ContextData.hpp"
#include "providers/ClaudeCacheControl.hpp"

#include <QJsonArray>
#include <QTest>
#include <QTextDocument>

namespace QodeAssist {

namespace {

constexpr char kTestMimeType[] = "text/python";
constexpr char kTestFilePath[] = "/path/to/file";
constexpr char kTestFileContext[]
    = "\n Language:  (MIME: text/python) filepath: /path/to/file()\n\n";

} // namespace

Context::DocumentContextReader QodeAssistTest::createReader(const QString &text)
{
    auto *document = new QTextDocument(this);
    document->setPlainText(text);
    return Context::DocumentContextReader(document, kTestMimeType, kTestFilePath);
}

QSharedPointer<Settings::CodeCompletionSettings> QodeAssistTest::createSettingsForWholeFile()
{
    auto settings = QSharedPointer<Settings::CodeCompletionSettings>::create();
    settings->readFullFile.setValue(true, Utils::BaseAspect::BeQuiet);
    settings->useProjectChangesCache.setValue(false, Utils::BaseAspect::BeQuiet);
    return settings;
}

QSharedPointer<Settings::CodeCompletionSettings> QodeAssistTest::createSettingsForLines(
    int linesBefore, int linesAfter)
{
    auto settings = QSharedPointer<Settings::CodeCompletionSettings>::create();
    settings->readFullFile.setValue(false, Utils::BaseAspect::BeQuiet);
    settings->readStringsBeforeCursor.setValue(linesBefore, Utils::BaseAspect::BeQuiet);
    settings->readStringsAfterCursor.setValue(linesAfter, Utils::BaseAspect::BeQuiet);
    settings->useProjectChangesCache.setValue(false, Utils::BaseAspect::BeQuiet);
    return settings;
}

QJsonObject QodeAssistTest::expectedEphemeral(bool extendedTtl)
{
    QJsonObject cacheControl{{"type", "ephemeral"}};
    if (extendedTtl)
        cacheControl["ttl"] = "1h";
    return cacheControl;
}

void QodeAssistTest::testProcessTextEmpty()
{
    QCOMPARE(CodeHandler::processText("", "/file.py"), QString("\n\n"));
}

void QodeAssistTest::testProcessTextCommentsAroundCodeBlock()
{
    const QString input = "This is a comment\n"
                          "```python\nprint('Hello, world!')\n```\n"
                          "Another comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is a comment\n\nprint('Hello, world!')\n# Another comment\n\n"));
}

void QodeAssistTest::testProcessTextWithPlainCodeBlockNoNewline()
{
    const QString input = "This is a comment\n"
                          "```print('Hello, world!')\n```\n"
                          "Another comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is a comment\n\nprint('Hello, world!')\n# Another comment\n\n"));
}

void QodeAssistTest::testProcessTextWithPlainCodeBlockWithNewline()
{
    const QString input = "This is a comment\n"
                          "```\nprint('Hello, world!')\n```\n"
                          "Another comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is a comment\n\n\nprint('Hello, world!')\n# Another comment\n\n"));
}

void QodeAssistTest::testProcessTextNoCommentsWithLanguageCodeBlock()
{
    const QString input = "```python\nprint('Hello, world!')\n```";

    QCOMPARE(CodeHandler::processText(input, "/file.py"), QString("print('Hello, world!')\n"));
}

void QodeAssistTest::testProcessTextNoCommentsWithPlainCodeBlockNoNewline()
{
    const QString input = "```print('Hello, world!')\n```";

    QCOMPARE(CodeHandler::processText(input, "/file.py"), QString("print('Hello, world!')\n"));
}

void QodeAssistTest::testProcessTextNoCommentsWithPlainCodeBlockWithNewline()
{
    const QString input = "```\nprint('Hello, world!')\n```";

    QCOMPARE(CodeHandler::processText(input, "/file.py"), QString("\nprint('Hello, world!')\n"));
}

void QodeAssistTest::testProcessTextWithMultipleCodeBlocksDifferentLanguages()
{
    const QString input = "First comment\n```python\nprint('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```cpp\ncout << \"Block 2\";\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n"
            "print('Block 1')\n"
            "// Middle comment\n\n"
            "cout << \"Block 2\";\n"
            "// Last comment\n\n"));
}

void QodeAssistTest::testProcessTextWithMultipleCodeBlocksSameLanguage()
{
    const QString input = "First comment\n```python\nprint('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```python\nprint('Block 2')\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n"
            "print('Block 1')\n"
            "# Middle comment\n\n"
            "print('Block 2')\n"
            "# Last comment\n\n"));
}

void QodeAssistTest::testProcessTextWithMultiplePlainCodeBlocksWithNewline()
{
    const QString input = "First comment\n```\nprint('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```\ncout << \"Block 2\";\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n\n"
            "print('Block 1')\n"
            "# Middle comment\n\n\n"
            "cout << \"Block 2\";\n"
            "# Last comment\n\n"));
}

void QodeAssistTest::testProcessTextWithMultiplePlainCodeBlocksWithoutNewline()
{
    const QString input = "First comment\n```print('Block 1')\n"
                          "```\nMiddle comment\n"
                          "```cout << \"Block 2\";\n```\n"
                          "Last comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# First comment\n\n"
            "print('Block 1')\n"
            "# Middle comment\n\n"
            "cout << \"Block 2\";\n"
            "# Last comment\n\n"));
}

void QodeAssistTest::testProcessTextWithEmptyLines()
{
    const QString input
        = "Comment with empty line\n\n```python\nprint('Hello')\n```\n\nAnother comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# Comment with empty line\n\n\n"
            "print('Hello')\n\n"
            "# Another comment\n\n"));
}

void QodeAssistTest::testProcessTextPlainCodeBlockWithNewlineWithEmptyLines()
{
    const QString input = "Comment with empty line\n\n```\nprint('Hello')\n```\n\nAnother comment";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString(
            "# Comment with empty line\n\n\n\n"
            "print('Hello')\n\n"
            "# Another comment\n\n"));
}

void QodeAssistTest::testProcessTextWithoutCodeBlock()
{
    const QString input = "This is just a comment\nwith multiple lines";

    QCOMPARE(
        CodeHandler::processText(input, "/file.py"),
        QString("# This is just a comment\n# with multiple lines\n\n"));
}

void QodeAssistTest::testDetectLanguageFromLine()
{
    QCOMPARE(CodeHandler::detectLanguageFromLine("```python"), QString("python"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("```javascript"), QString("js"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("```cpp"), QString("c-like"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("``` ruby "), QString("ruby"));
    QCOMPARE(CodeHandler::detectLanguageFromLine("```"), QString());
    QCOMPARE(CodeHandler::detectLanguageFromLine("``` "), QString());
}

void QodeAssistTest::testDetectLanguageFromExtension()
{
    QCOMPARE(CodeHandler::detectLanguageFromExtension("py"), QString("python"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("js"), QString("js"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("cpp"), QString("c-like"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("hpp"), QString("c-like"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("rb"), QString("ruby"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("sh"), QString("shell"));
    QCOMPARE(CodeHandler::detectLanguageFromExtension("unknown"), QString());
    QCOMPARE(CodeHandler::detectLanguageFromExtension(""), QString());
}

void QodeAssistTest::testCommentPrefixForDifferentLanguages()
{
    struct Case
    {
        QString input;
        QString expected;
    };

    const QList<Case> cases{
        {"Comment\n```python\ncode\n```", "# Comment\n\ncode\n"},
        {"Comment\n```cpp\ncode\n```", "// Comment\n\ncode\n"},
        {"Comment\n```ruby\ncode\n```", "# Comment\n\ncode\n"},
        {"Comment\n```lua\ncode\n```", "-- Comment\n\ncode\n"}};

    for (const auto &testCase : cases)
        QCOMPARE(CodeHandler::processText(testCase.input, ""), testCase.expected);
}

void QodeAssistTest::testHasCodeBlocks()
{
    QVERIFY(CodeHandler::hasCodeBlocks("text\n```python\ncode\n```"));
    QVERIFY(!CodeHandler::hasCodeBlocks("just a plain comment\nwith two lines"));
}

void QodeAssistTest::testReplaceLengthEmptyRight()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("foo", ""), 0);
}

void QodeAssistTest::testReplaceLengthNoOverlap()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("foo", "bar"), 0);
}

void QodeAssistTest::testReplaceLengthSingleCharNoOverlap()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("a", "b"), 0);
}

void QodeAssistTest::testReplaceLengthLcpExtendsRight()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("myVar", "myVariable"), 5);
}

void QodeAssistTest::testReplaceLengthLcpPartialEngineCall()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("engine.load()", "engine.rootContext()"), 7);
}

void QodeAssistTest::testReplaceLengthLcpThenClosingTailExtendsFull()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("colors << \"red\"", "colors{};"), 9);
}

void QodeAssistTest::testReplaceLengthClosingTailReplaceSemicolon()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("x;", ";"), 1);
}

void QodeAssistTest::testReplaceLengthClosingTailReplaceParen()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("arg1, arg2)", ")"), 1);
}

void QodeAssistTest::testReplaceLengthClosingTailReplaceBrackets()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("[0]", "];"), 2);
}

void QodeAssistTest::testReplaceLengthClosingTailReplaceBracesAndSemi()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("= {\"red\"}", "{};"), 3);
}

void QodeAssistTest::testReplaceLengthClosingTailWithLeadingSpace()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength(" = {\"red\"}", "{};"), 3);
}

void QodeAssistTest::testReplaceLengthClosingTailEqualsSemi()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("= 5;", ";"), 1);
}

void QodeAssistTest::testReplaceLengthClosingTailNoMatchingClose()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("\"red\", \"green\"", "};"), 0);
}

void QodeAssistTest::testReplaceLengthRealCodeRightNoLcp()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("myVar", " = 5;"), 0);
}

void QodeAssistTest::testReplaceLengthTrailingWhitespaceOnlyLeftAlone()
{
    QCOMPARE(LLMSuggestion::calculateReplaceLength("code", "   "), 0);
}

void QodeAssistTest::testCacheControlBreakpointWithoutExtendedTtl()
{
    const QJsonObject cacheControl = Providers::ClaudeCacheControl::buildBreakpoint(false);

    QCOMPARE(cacheControl.value("type").toString(), QString("ephemeral"));
    QVERIFY(!cacheControl.contains("ttl"));
}

void QodeAssistTest::testCacheControlBreakpointWithExtendedTtl()
{
    const QJsonObject cacheControl = Providers::ClaudeCacheControl::buildBreakpoint(true);

    QCOMPARE(cacheControl.value("type").toString(), QString("ephemeral"));
    QCOMPARE(cacheControl.value("ttl").toString(), QString("1h"));
}

void QodeAssistTest::testCacheControlSystemAsStringWrappedIntoArray()
{
    QJsonObject request;
    request["system"] = "you are a helpful agent";

    Providers::ClaudeCacheControl::apply(request, false);

    QVERIFY(request.value("system").isArray());
    const QJsonArray system = request.value("system").toArray();
    QCOMPARE(system.size(), 1);

    const QJsonObject block = system.first().toObject();
    QCOMPARE(block.value("type").toString(), QString("text"));
    QCOMPARE(block.value("text").toString(), QString("you are a helpful agent"));
    QCOMPARE(block.value("cache_control").toObject(), expectedEphemeral(false));
}

void QodeAssistTest::testCacheControlEmptySystemStringIsNotWrapped()
{
    QJsonObject request;
    request["system"] = "";

    Providers::ClaudeCacheControl::apply(request, false);

    QVERIFY(request.value("system").isString());
}

void QodeAssistTest::testCacheControlSystemAsArrayMarksLastBlock()
{
    QJsonObject request;
    request["system"] = QJsonArray{
        QJsonObject{{"type", "text"}, {"text", "a"}}, QJsonObject{{"type", "text"}, {"text", "b"}}};

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray system = request.value("system").toArray();
    QCOMPARE(system.size(), 2);
    QVERIFY(!system[0].toObject().contains("cache_control"));
    QCOMPARE(system[1].toObject().value("cache_control").toObject(), expectedEphemeral(false));
}

void QodeAssistTest::testCacheControlToolsLastEntryGetsCacheControl()
{
    QJsonObject request;
    request["tools"] = QJsonArray{
        QJsonObject{{"name", "read_file"}},
        QJsonObject{{"name", "edit_file"}},
        QJsonObject{{"name", "search"}}};

    Providers::ClaudeCacheControl::apply(request, true);

    const QJsonArray tools = request.value("tools").toArray();
    QCOMPARE(tools.size(), 3);
    QVERIFY(!tools[0].toObject().contains("cache_control"));
    QVERIFY(!tools[1].toObject().contains("cache_control"));
    QCOMPARE(tools[2].toObject().value("cache_control").toObject(), expectedEphemeral(true));
}

void QodeAssistTest::testCacheControlSingleMessageHistorySkipped()
{
    QJsonObject request;
    request["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", "first message"}}};

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray messages = request.value("messages").toArray();
    QCOMPARE(messages.size(), 1);
    QVERIFY(messages[0].toObject().value("content").isString());
}

void QodeAssistTest::testCacheControlHistoryBreakpointOnSecondToLastMessage()
{
    QJsonObject request;
    request["messages"] = QJsonArray{
        QJsonObject{{"role", "user"}, {"content", "u1"}},
        QJsonObject{{"role", "assistant"}, {"content", "a1"}},
        QJsonObject{{"role", "user"}, {"content", "u2-current"}}};

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray messages = request.value("messages").toArray();
    QCOMPARE(messages.size(), 3);
    QVERIFY(messages[0].toObject().value("content").isString());

    const QJsonArray assistantContent = messages[1].toObject().value("content").toArray();
    QCOMPARE(assistantContent.size(), 1);
    QCOMPARE(assistantContent.first().toObject().value("text").toString(), QString("a1"));
    QCOMPARE(
        assistantContent.first().toObject().value("cache_control").toObject(),
        expectedEphemeral(false));

    QVERIFY(messages[2].toObject().value("content").isString());
}

void QodeAssistTest::testCacheControlHistoryArrayContentMarksLastBlock()
{
    QJsonObject request;
    request["messages"] = QJsonArray{
        QJsonObject{
            {"role", "user"},
            {"content",
             QJsonArray{
                 QJsonObject{{"type", "text"}, {"text", "describe this"}},
                 QJsonObject{{"type", "image"}}}}},
        QJsonObject{{"role", "assistant"}, {"content", "ok"}}};

    Providers::ClaudeCacheControl::apply(request, false);

    const QJsonArray messages = request.value("messages").toArray();
    const QJsonArray content = messages[0].toObject().value("content").toArray();
    QCOMPARE(content.size(), 2);
    QVERIFY(!content[0].toObject().contains("cache_control"));
    QCOMPARE(content[1].toObject().value("cache_control").toObject(), expectedEphemeral(false));
}

void QodeAssistTest::testCacheControlNoSystemNoToolsNoMessagesIsNoop()
{
    QJsonObject request;
    request["model"] = "claude-sonnet-4-5";
    request["max_tokens"] = 1024;

    Providers::ClaudeCacheControl::apply(request, false);

    QCOMPARE(request.value("model").toString(), QString("claude-sonnet-4-5"));
    QCOMPARE(request.value("max_tokens").toInt(), 1024);
    QVERIFY(!request.contains("system"));
    QVERIFY(!request.contains("tools"));
    QVERIFY(!request.contains("messages"));
}

void QodeAssistTest::testCacheControlEmptyToolsArrayIsNoop()
{
    QJsonObject request;
    request["tools"] = QJsonArray{};

    Providers::ClaudeCacheControl::apply(request, false);

    QVERIFY(request.value("tools").isArray());
    QVERIFY(request.value("tools").toArray().isEmpty());
}

void QodeAssistTest::testGetLineText()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2");

    QCOMPARE(reader.getLineText(0), QString("Line 0"));
    QCOMPARE(reader.getLineText(1), QString("Line 1"));
    QCOMPARE(reader.getLineText(2), QString("Line 2"));
    QCOMPARE(reader.getLineText(0, 4), QString("Line"));
}

void QodeAssistTest::testGetContext()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(reader.getContextBefore(0, -1, 2), QString("Line 0"));
    QCOMPARE(reader.getContextAfter(0, -1, 2), QString("Line 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(1, -1, 2), QString("Line 0\nLine 1"));
    QCOMPARE(reader.getContextAfter(1, -1, 2), QString("Line 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(2, -1, 2), QString("Line 1\nLine 2"));
    QCOMPARE(reader.getContextAfter(2, -1, 2), QString("Line 2\nLine 3"));

    QCOMPARE(reader.getContextBefore(3, -1, 2), QString("Line 2\nLine 3"));
    QCOMPARE(reader.getContextAfter(3, -1, 2), QString("Line 3\nLine 4"));

    QCOMPARE(reader.getContextBefore(0, 1, 2), QString("L"));
    QCOMPARE(reader.getContextAfter(0, 1, 2), QString("ine 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(1, 1, 2), QString("Line 0\nL"));
    QCOMPARE(reader.getContextAfter(1, 1, 2), QString("ine 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(2, 1, 2), QString("Line 1\nL"));
    QCOMPARE(reader.getContextAfter(2, 1, 2), QString("ine 2\nLine 3"));

    QCOMPARE(reader.getContextBefore(3, 1, 2), QString("Line 2\nL"));
    QCOMPARE(reader.getContextAfter(3, 1, 2), QString("ine 3\nLine 4"));
}

void QodeAssistTest::testGetContextWithCopyright()
{
    auto reader = createReader("/* Copyright (C) 2024 */\nLine 0\nLine 1\nLine 2\nLine 3");

    QCOMPARE(reader.getContextBefore(0, -1, 2), QString());
    QCOMPARE(reader.getContextAfter(0, -1, 2), QString("Line 0"));

    QCOMPARE(reader.getContextBefore(1, -1, 2), QString("Line 0"));
    QCOMPARE(reader.getContextAfter(1, -1, 2), QString("Line 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(2, -1, 2), QString("Line 0\nLine 1"));
    QCOMPARE(reader.getContextAfter(2, -1, 2), QString("Line 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(3, -1, 2), QString("Line 1\nLine 2"));
    QCOMPARE(reader.getContextAfter(3, -1, 2), QString("Line 2\nLine 3"));

    QCOMPARE(reader.getContextBefore(0, 1, 2), QString());
    QCOMPARE(reader.getContextAfter(0, 1, 2), QString("Line 0"));

    QCOMPARE(reader.getContextBefore(1, 1, 2), QString("L"));
    QCOMPARE(reader.getContextAfter(1, 1, 2), QString("ine 0\nLine 1"));

    QCOMPARE(reader.getContextBefore(2, 1, 2), QString("Line 0\nL"));
    QCOMPARE(reader.getContextAfter(2, 1, 2), QString("ine 1\nLine 2"));

    QCOMPARE(reader.getContextBefore(3, 1, 2), QString("Line 1\nL"));
    QCOMPARE(reader.getContextAfter(3, 1, 2), QString("ine 2\nLine 3"));
}

void QodeAssistTest::testReadWholeFile()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(reader.readWholeFileBefore(0, -1), QString("Line 0"));
    QCOMPARE(reader.readWholeFileAfter(0, -1), QString("Line 0\nLine 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(1, -1), QString("Line 0\nLine 1"));
    QCOMPARE(reader.readWholeFileAfter(1, -1), QString("Line 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(2, -1), QString("Line 0\nLine 1\nLine 2"));
    QCOMPARE(reader.readWholeFileAfter(2, -1), QString("Line 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(3, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));
    QCOMPARE(reader.readWholeFileAfter(3, -1), QString("Line 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(4, -1), QString("Line 0\nLine 1\nLine 2\nLine 3\nLine 4"));
    QCOMPARE(reader.readWholeFileAfter(4, -1), QString("Line 4"));

    QCOMPARE(reader.readWholeFileBefore(0, 1), QString("L"));
    QCOMPARE(reader.readWholeFileAfter(0, 1), QString("ine 0\nLine 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(1, 1), QString("Line 0\nL"));
    QCOMPARE(reader.readWholeFileAfter(1, 1), QString("ine 1\nLine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(2, 1), QString("Line 0\nLine 1\nL"));
    QCOMPARE(reader.readWholeFileAfter(2, 1), QString("ine 2\nLine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(3, 1), QString("Line 0\nLine 1\nLine 2\nL"));
    QCOMPARE(reader.readWholeFileAfter(3, 1), QString("ine 3\nLine 4"));

    QCOMPARE(reader.readWholeFileBefore(4, 1), QString("Line 0\nLine 1\nLine 2\nLine 3\nL"));
    QCOMPARE(reader.readWholeFileAfter(4, 1), QString("ine 4"));
}

void QodeAssistTest::testReadWholeFileWithCopyright()
{
    auto reader = createReader("/* Copyright (C) 2024 */\nLine 0\nLine 1\nLine 2\nLine 3");

    QCOMPARE(reader.readWholeFileBefore(0, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, -1), QString("Line 0"));
    QCOMPARE(reader.readWholeFileAfter(1, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, -1), QString("Line 0\nLine 1"));
    QCOMPARE(reader.readWholeFileAfter(2, -1), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, -1), QString("Line 0\nLine 1\nLine 2"));
    QCOMPARE(reader.readWholeFileAfter(3, -1), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(1, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 0), QString("Line 0\n"));
    QCOMPARE(reader.readWholeFileAfter(2, 0), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 0), QString("Line 0\nLine 1\n"));
    QCOMPARE(reader.readWholeFileAfter(3, 0), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 1), QString("L"));
    QCOMPARE(reader.readWholeFileAfter(1, 1), QString("ine 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 1), QString("Line 0\nL"));
    QCOMPARE(reader.readWholeFileAfter(2, 1), QString("ine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 1), QString("Line 0\nLine 1\nL"));
    QCOMPARE(reader.readWholeFileAfter(3, 1), QString("ine 2\nLine 3"));
}

void QodeAssistTest::testReadWholeFileWithMultilineCopyright()
{
    auto reader = createReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\n"
        "Line 0\nLine 1\nLine 2\nLine 3");

    QCOMPARE(reader.readWholeFileBefore(0, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(1, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(2, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(3, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(4, -1), QString());
    QCOMPARE(reader.readWholeFileAfter(4, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(5, -1), QString("Line 0"));
    QCOMPARE(reader.readWholeFileAfter(5, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(6, -1), QString("Line 0\nLine 1"));
    QCOMPARE(reader.readWholeFileAfter(6, -1), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(7, -1), QString("Line 0\nLine 1\nLine 2"));
    QCOMPARE(reader.readWholeFileAfter(7, -1), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(8, -1), QString("Line 0\nLine 1\nLine 2\nLine 3"));
    QCOMPARE(reader.readWholeFileAfter(8, -1), QString("Line 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(1, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(2, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(3, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(4, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(4, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(5, 0), QString());
    QCOMPARE(reader.readWholeFileAfter(5, 0), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(6, 0), QString("Line 0\n"));
    QCOMPARE(reader.readWholeFileAfter(6, 0), QString("Line 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(7, 0), QString("Line 0\nLine 1\n"));
    QCOMPARE(reader.readWholeFileAfter(7, 0), QString("Line 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(8, 0), QString("Line 0\nLine 1\nLine 2\n"));
    QCOMPARE(reader.readWholeFileAfter(8, 0), QString("Line 3"));

    QCOMPARE(reader.readWholeFileBefore(0, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(0, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(1, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(1, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(2, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(2, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(3, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(3, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(4, 1), QString());
    QCOMPARE(reader.readWholeFileAfter(4, 1), QString("Line 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(5, 1), QString("L"));
    QCOMPARE(reader.readWholeFileAfter(5, 1), QString("ine 0\nLine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(6, 1), QString("Line 0\nL"));
    QCOMPARE(reader.readWholeFileAfter(6, 1), QString("ine 1\nLine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(7, 1), QString("Line 0\nLine 1\nL"));
    QCOMPARE(reader.readWholeFileAfter(7, 1), QString("ine 2\nLine 3"));

    QCOMPARE(reader.readWholeFileBefore(8, 1), QString("Line 0\nLine 1\nLine 2\nL"));
    QCOMPARE(reader.readWholeFileAfter(8, 1), QString("ine 3"));
}

void QodeAssistTest::testFindCopyrightSingleLine()
{
    auto reader = createReader("/* Copyright (C) 2024 */\nCode line 0\nCode line 1");

    const auto info = reader.findCopyright();
    QVERIFY(info.found);
    QCOMPARE(info.startLine, 0);
    QCOMPARE(info.endLine, 0);
}

void QodeAssistTest::testFindCopyrightMultiLine()
{
    auto reader = createReader(
        "/*\n * Copyright (C) 2024\n * \n * This file is part of QodeAssist.\n */\nCode line 0");

    const auto info = reader.findCopyright();
    QVERIFY(info.found);
    QCOMPARE(info.startLine, 0);
    QCOMPARE(info.endLine, 4);
}

void QodeAssistTest::testFindCopyrightMultipleBlocks()
{
    auto reader = createReader("/* Copyright 2023 */\n\n/* Copyright 2024 */\nCode");

    const auto info = reader.findCopyright();
    QVERIFY(info.found);
    QCOMPARE(info.startLine, 0);
    QCOMPARE(info.endLine, 0);
}

void QodeAssistTest::testFindCopyrightNoCopyright()
{
    auto reader = createReader("/* Just a comment */\nCode line 0");

    const auto info = reader.findCopyright();
    QVERIFY(!info.found);
    QCOMPARE(info.startLine, -1);
    QCOMPARE(info.endLine, -1);
}

void QodeAssistTest::testGetContextBetween()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(reader.getContextBetween(2, -1, 0, -1), QString());
    QCOMPARE(reader.getContextBetween(0, -1, 0, -1), QString("Line 0"));
    QCOMPARE(reader.getContextBetween(1, -1, 1, -1), QString("Line 1"));
    QCOMPARE(reader.getContextBetween(1, 3, 1, 1), QString());
    QCOMPARE(reader.getContextBetween(1, 3, 1, 3), QString());
    QCOMPARE(reader.getContextBetween(1, 3, 1, 4), QString("e"));

    QCOMPARE(reader.getContextBetween(1, -1, 3, -1), QString("Line 1\nLine 2\nLine 3"));
    QCOMPARE(reader.getContextBetween(1, 2, 3, -1), QString("ne 1\nLine 2\nLine 3"));
    QCOMPARE(reader.getContextBetween(1, -1, 3, 2), QString("Line 1\nLine 2\nLi"));
    QCOMPARE(reader.getContextBetween(1, 2, 3, 2), QString("ne 1\nLine 2\nLi"));
}

void QodeAssistTest::testPrepareContext()
{
    auto reader = createReader("Line 0\nLine 1\nLine 2\nLine 3\nLine 4");

    QCOMPARE(
        reader.prepareContext(2, 3, *createSettingsForWholeFile()),
        (LLMCore::ContextData{
            .prefix = "Line 0\nLine 1\nLin",
            .suffix = "e 2\nLine 3\nLine 4",
            .fileContext = kTestFileContext}));

    QCOMPARE(
        reader.prepareContext(2, 3, *createSettingsForLines(1, 1)),
        (LLMCore::ContextData{
            .prefix = "Line 1\nLin", .suffix = "e 2\nLine 3", .fileContext = kTestFileContext}));

    QCOMPARE(
        reader.prepareContext(2, 3, *createSettingsForLines(2, 2)),
        (LLMCore::ContextData{
            .prefix = "Line 0\nLine 1\nLin",
            .suffix = "e 2\nLine 3\nLine 4",
            .fileContext = kTestFileContext}));
}

} // namespace QodeAssist
