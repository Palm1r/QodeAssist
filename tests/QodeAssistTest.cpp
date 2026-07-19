// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "QodeAssistTest.hpp"

#include "ChatView/ChatHistoryBridge.hpp"
#include "ChatView/ChatModel.hpp"
#include "completion/CodeHandler.hpp"
#include "completion/LLMSuggestion.hpp"
#include "llmcore/ContextData.hpp"
#include "providers/ClaudeCacheControl.hpp"
#include "session/HistoryProjection.hpp"
#include "session/HistorySerializer.hpp"
#include "session/TurnContextBuilder.hpp"

#include <QJsonArray>
#include <QTest>
#include <QTextDocument>

namespace QodeAssist {

namespace {

constexpr char kTestMimeType[] = "text/python";
constexpr char kTestFilePath[] = "/path/to/file";
constexpr char kTestFileContext[]
    = "\n Language:  (MIME: text/python) filepath: /path/to/file()\n\n";

Session::Message userTurn()
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

Session::Message assistantTurn()
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

Session::ConversationHistory sampleHistory()
{
    Session::Message assistant = assistantTurn();
    assistant.blocks.append(
        Session::FileEditBlock{"e1", "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\"}"});

    Session::ConversationHistory history;
    history.append(userTurn());
    history.append(assistant);
    return history;
}

Session::ConversationHistory historyWithoutFileEdits()
{
    Session::ConversationHistory history;
    history.append(userTurn());
    history.append(assistantTurn());
    return history;
}

class FakeProjectContext : public Session::IProjectContextPort
{
public:
    FakeProjectContext(Session::ProjectInfo info, QString rules)
        : m_info(std::move(info))
        , m_rules(std::move(rules))
    {}

    Session::ProjectInfo projectInfo() const override { return m_info; }
    QString projectRules() const override { return m_rules; }

private:
    Session::ProjectInfo m_info;
    QString m_rules;
};

class FakeSkillsContext : public Session::ISkillsContextPort
{
public:
    FakeSkillsContext(QString alwaysOn, QString catalog, QList<Session::InvokedSkill> skills)
        : m_alwaysOn(std::move(alwaysOn))
        , m_catalog(std::move(catalog))
        , m_skills(std::move(skills))
    {}

    QString alwaysOnBodies() const override { return m_alwaysOn; }
    QString catalogText() const override { return m_catalog; }

    std::optional<Session::InvokedSkill> findSkill(const QString &name) const override
    {
        for (const Session::InvokedSkill &skill : m_skills) {
            if (skill.name == name)
                return skill;
        }
        return std::nullopt;
    }

private:
    QString m_alwaysOn;
    QString m_catalog;
    QList<Session::InvokedSkill> m_skills;
};

class FakeLinkedFiles : public Session::ILinkedFilesPort
{
public:
    explicit FakeLinkedFiles(QList<Session::LinkedFile> files)
        : m_files(std::move(files))
    {}

    QList<Session::LinkedFile> readFiles(const QList<QString> &paths) const override
    {
        m_requestedPaths = paths;
        return m_files;
    }

    QList<QString> requestedPaths() const { return m_requestedPaths; }

private:
    QList<Session::LinkedFile> m_files;
    mutable QList<QString> m_requestedPaths;
};

Session::ProjectInfo sampleProject()
{
    Session::ProjectInfo info;
    info.available = true;
    info.displayName = "QodeAssist";
    info.sourceRoot = "/src/QodeAssist";
    info.buildDirectory = "/src/QodeAssist/build";
    return info;
}

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

void QodeAssistTest::testHistorySnapshotUsesCurrentVersion()
{
    const QJsonObject root = Session::HistorySerializer::toJson(sampleHistory());

    QCOMPARE(root.value("version").toString(), Session::HistorySerializer::currentVersion());
    QCOMPARE(root.value("messages").toArray().size(), 2);
}

void QodeAssistTest::testHistorySnapshotRoundTrip()
{
    const Session::ConversationHistory history = sampleHistory();

    const auto restored = Session::HistorySerializer::fromJson(
        Session::HistorySerializer::toJson(history));

    QVERIFY(restored.has_value());
    QCOMPARE(restored->size(), history.size());
    QCOMPARE(restored->at(1).blocks.size(), history.at(1).blocks.size());
    QCOMPARE(*restored, history);
}

void QodeAssistTest::testUnsupportedChatVersionIsRejected()
{
    QJsonObject root;
    root["version"] = "9.9";
    root["messages"] = QJsonArray{};

    QVERIFY(!Session::HistorySerializer::fromJson(root).has_value());
    QVERIFY(!Session::HistorySerializer::isSupportedVersion("9.9"));
    QVERIFY(Session::HistorySerializer::isSupportedVersion("0.1"));
    QVERIFY(Session::HistorySerializer::isSupportedVersion("0.2"));
}

void QodeAssistTest::testChatFileWithoutMessagesArrayIsRejected()
{
    QJsonObject missing;
    missing["version"] = Session::HistorySerializer::currentVersion();
    QVERIFY(!Session::HistorySerializer::fromJson(missing).has_value());

    QJsonObject wrongType;
    wrongType["version"] = Session::HistorySerializer::currentVersion();
    wrongType["messages"] = "not an array";
    QVERIFY(!Session::HistorySerializer::fromJson(wrongType).has_value());

    QJsonObject empty;
    empty["version"] = Session::HistorySerializer::currentVersion();
    empty["messages"] = QJsonArray{};
    const auto history = Session::HistorySerializer::fromJson(empty);
    QVERIFY(history.has_value());
    QCOMPARE(history->size(), 0);
}

void QodeAssistTest::testLegacyChatFileConvertsToHistory()
{
    QJsonObject root;
    root["version"] = "0.2";
    root["messages"] = QJsonArray{
        QJsonObject{{"role", 1}, {"content", "explain this"}, {"id", "u1"}},
        QJsonObject{
            {"role", 5},
            {"content", "weighing options\n[Signature: sig-1...]"},
            {"id", "r1"},
            {"signature", "sig-1"},
            {"usage", QJsonObject{{"promptTokens", 120}, {"completionTokens", 40}}}},
        QJsonObject{
            {"role", 3},
            {"content", "read_file\nint main() {}"},
            {"id", "t1"},
            {"toolName", "read_file"},
            {"toolResult", "int main() {}"}},
        QJsonObject{{"role", 2}, {"content", "here is the answer"}, {"id", "r1"}}};

    const auto history = Session::HistorySerializer::fromJson(root);

    QVERIFY(history.has_value());
    QCOMPARE(history->size(), 2);

    const Session::Message &user = history->at(0);
    QCOMPARE(user.role, Session::MessageRole::User);
    QCOMPARE(user.blocks.size(), 1);
    QCOMPARE(std::get<Session::TextBlock>(user.blocks.at(0)), (Session::TextBlock{"explain this"}));

    const Session::Message &assistant = history->at(1);
    QCOMPARE(assistant.role, Session::MessageRole::Assistant);
    QCOMPARE(assistant.id, QString("r1"));
    QCOMPARE(assistant.blocks.size(), 3);
    QCOMPARE(
        std::get<Session::ThinkingBlock>(assistant.blocks.at(0)),
        (Session::ThinkingBlock{"weighing options", "sig-1", false}));
    QCOMPARE(
        std::get<Session::ToolCallBlock>(assistant.blocks.at(1)),
        (Session::ToolCallBlock{"t1", "read_file", QJsonObject{}, "int main() {}"}));
    QCOMPARE(
        std::get<Session::TextBlock>(assistant.blocks.at(2)),
        (Session::TextBlock{"here is the answer"}));
    QCOMPARE(assistant.usage.promptTokens, 120);
    QCOMPARE(assistant.usage.completionTokens, 40);
}

void QodeAssistTest::testLegacyToolRowWithoutMetadataKeepsItsText()
{
    QJsonObject root;
    root["version"] = "0.2";
    root["messages"] = QJsonArray{
        QJsonObject{{"role", 1}, {"content", "list the files"}, {"id", "u1"}},
        QJsonObject{{"role", 3}, {"content", "main.cpp\nmain.qml"}, {"id", "t1"}}};

    const auto history = Session::HistorySerializer::fromJson(root);
    QVERIFY(history.has_value());

    const QList<Session::MessageRow> rows = Session::projectToRows(*history);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(1).kind, Session::RowKind::Tool);
    QCOMPARE(rows.at(1).id, QString("t1"));
    QCOMPARE(rows.at(1).content, QString("main.cpp\nmain.qml"));
}

void QodeAssistTest::testChatRowsRoundTripThroughHistory()
{
    Session::MessageRow user;
    user.kind = Session::RowKind::User;
    user.id = "u1";
    user.content = "explain this";
    user.attachments = {Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}};
    user.images = {Session::ImageBlock{"shot.png", "shot_cd34.png", "image/png"}};

    Session::MessageRow thinking;
    thinking.kind = Session::RowKind::Thinking;
    thinking.id = "r1";
    thinking.content = "weighing options\n[Signature: sig-1...]";
    thinking.signature = "sig-1";
    thinking.usage = Session::Usage{120, 40, 80, 12};

    Session::MessageRow tool;
    tool.kind = Session::RowKind::Tool;
    tool.id = "t1";
    tool.content = "read_file\nint main() {}";
    tool.toolName = "read_file";
    tool.toolArguments = QJsonObject{{"path", "main.cpp"}};
    tool.toolResult = "int main() {}";

    Session::MessageRow assistant;
    assistant.kind = Session::RowKind::Assistant;
    assistant.id = "r1";
    assistant.content = "here is the answer";

    Session::MessageRow fileEdit;
    fileEdit.kind = Session::RowKind::FileEdit;
    fileEdit.id = "e1";
    fileEdit.content = "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\"}";

    const QList<Session::MessageRow> rows{user, thinking, tool, assistant, fileEdit};

    const Session::ConversationHistory history = Session::buildFromRows(rows);
    QCOMPARE(history.size(), 2);

    const QList<Session::MessageRow> projected = Session::projectToRows(history);
    QCOMPARE(projected, rows);
}

void QodeAssistTest::testCompressedChatShapeReloadsAsOneAssistantRow()
{
    Session::Message summary;
    summary.role = Session::MessageRole::Assistant;
    summary.id = "c1";
    summary.blocks = {Session::TextBlock{"# Chat Summary\n\nwe discussed the parser"}};

    Session::ConversationHistory compressed;
    compressed.append(summary);

    const auto reloaded = Session::HistorySerializer::fromJson(
        Session::HistorySerializer::toJson(compressed));
    QVERIFY(reloaded.has_value());

    const QList<Session::MessageRow> rows = Session::projectToRows(*reloaded);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).kind, Session::RowKind::Assistant);
    QCOMPARE(rows.at(0).content, QString("# Chat Summary\n\nwe discussed the parser"));
}

void QodeAssistTest::testHistoryAppliesToChatModel()
{
    Chat::ChatModel model;

    Chat::ChatHistoryBridge::applyHistory(&model, historyWithoutFileEdits());

    QCOMPARE(model.rowCount(), 4);

    QCOMPARE(
        model.data(model.index(0), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::User);
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::Content).toString(), QString("explain this"));
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::Attachments).toList().size(), 1);
    QCOMPARE(model.data(model.index(0), Chat::ChatModel::Images).toList().size(), 1);

    QCOMPARE(
        model.data(model.index(1), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::Thinking);
    QCOMPARE(
        model.data(model.index(1), Chat::ChatModel::Content).toString(),
        QString("weighing options\n[Signature: sig-1...]"));

    QCOMPARE(
        model.data(model.index(2), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::Tool);
    QCOMPARE(
        model.data(model.index(2), Chat::ChatModel::Content).toString(),
        QString("read_file\nint main() {}"));

    QCOMPARE(
        model.data(model.index(3), Chat::ChatModel::RoleType).value<Chat::ChatModel::ChatRole>(),
        Chat::ChatModel::ChatRole::Assistant);
    QCOMPARE(
        model.data(model.index(3), Chat::ChatModel::Content).toString(),
        QString("here is the answer"));

    QCOMPARE(model.sessionPromptTokens(), 120);
    QCOMPARE(model.sessionCompletionTokens(), 40);
    QCOMPARE(model.sessionCachedPromptTokens(), 80);
}

void QodeAssistTest::testAdjacentThinkingBlocksSurviveReload()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks
        = {Session::ThinkingBlock{"first", "sig-a", true},
           Session::ThinkingBlock{"second", "sig-b", true},
           Session::TextBlock{"the answer"}};

    Session::ConversationHistory history;
    history.append(assistant);

    Chat::ChatModel model;
    Chat::ChatHistoryBridge::applyHistory(&model, history);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(
        model.data(model.index(0), Chat::ChatModel::Content).toString(),
        QString("first\n[Signature: sig-a...]"));
    QCOMPARE(
        model.data(model.index(1), Chat::ChatModel::Content).toString(),
        QString("second\n[Signature: sig-b...]"));
    QCOMPARE(Chat::ChatHistoryBridge::readHistory(&model), history);
}

void QodeAssistTest::testChatModelRoundTripsThroughHistory()
{
    Chat::ChatModel source;
    source.addMessage("explain this", Chat::ChatModel::ChatRole::User, "");
    source.addThinkingBlock("r1", "weighing options", "sig-1");
    source.addToolExecutionStatus("r1", "t1", "read_file", QJsonObject{{"path", "main.cpp"}});
    source.updateToolResult("r1", "t1", "read_file", "int main() {}");
    source.addMessage("here is the answer", Chat::ChatModel::ChatRole::Assistant, "r1");
    source.setMessageUsage("r1", 120, 40, 80, 12);

    const Session::ConversationHistory history = Chat::ChatHistoryBridge::readHistory(&source);
    QCOMPARE(history.size(), 2);

    Chat::ChatModel restored;
    Chat::ChatHistoryBridge::applyHistory(&restored, history);

    QCOMPARE(restored.rowCount(), source.rowCount());
    for (int row = 0; row < source.rowCount(); ++row) {
        QCOMPARE(
            restored.data(restored.index(row), Chat::ChatModel::Content).toString(),
            source.data(source.index(row), Chat::ChatModel::Content).toString());
        QCOMPARE(
            restored.data(restored.index(row), Chat::ChatModel::RoleType)
                .value<Chat::ChatModel::ChatRole>(),
            source.data(source.index(row), Chat::ChatModel::RoleType)
                .value<Chat::ChatModel::ChatRole>());
    }

    QCOMPARE(restored.sessionTotalTokens(), source.sessionTotalTokens());
    QCOMPARE(Chat::ChatHistoryBridge::readHistory(&restored), history);
}

void QodeAssistTest::testTurnContextCollectsPartsFromPorts()
{
    FakeProjectContext projectPort(sampleProject(), "Always use tabs.");
    FakeSkillsContext skillsPort(
        "# Always on", "# Skills catalog", {Session::InvokedSkill{"review", "Review body"}});
    FakeLinkedFiles filesPort({Session::LinkedFile{"main.cpp", "int main() {}"}});

    Session::TurnContextRequest request;
    request.message = "please /review this";
    request.basePrompt = "You are a helpful assistant.";
    request.rolePrompt = "Act as a reviewer.";
    request.linkedFilePaths = QList<QString>{"/src/main.cpp"};

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, &skillsPort, filesPort).build(request);

    QCOMPARE(context.basePrompt, QString("You are a helpful assistant."));
    QVERIFY(context.rolePrompt.has_value());
    QCOMPARE(*context.rolePrompt, QString("Act as a reviewer."));
    QVERIFY(context.project.available);
    QCOMPARE(context.project.displayName, QString("QodeAssist"));
    QCOMPARE(context.projectRules, QString("Always use tabs."));
    QCOMPARE(context.alwaysOnSkills, QString("# Always on"));
    QCOMPARE(context.skillsCatalog, QString("# Skills catalog"));
    QCOMPARE(context.invokedSkills.size(), 1);
    QCOMPARE(context.invokedSkills.at(0).name, QString("review"));
    QCOMPARE(context.linkedFilePaths, (QList<QString>{"/src/main.cpp"}));
    QCOMPARE(context.linkedFiles.size(), 1);
    QCOMPARE(context.linkedFiles.at(0).fileName, QString("main.cpp"));
    QCOMPARE(filesPort.requestedPaths(), (QList<QString>{"/src/main.cpp"}));
}

void QodeAssistTest::testTurnContextSkillCommandScanning()
{
    FakeProjectContext projectPort({}, QString());
    FakeSkillsContext skillsPort(
        QString(),
        QString(),
        {Session::InvokedSkill{"review", "Review body"},
         Session::InvokedSkill{"docs", "Docs body"},
         Session::InvokedSkill{"blank", QString()}});
    FakeLinkedFiles filesPort({});

    Session::TurnContextRequest request;
    request.message = "/review then /docs then /review again /blank /unknown and mid/word";

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, &skillsPort, filesPort).build(request);

    QCOMPARE(context.invokedSkills.size(), 2);
    QCOMPARE(context.invokedSkills.at(0).name, QString("review"));
    QCOMPARE(context.invokedSkills.at(1).name, QString("docs"));
}

void QodeAssistTest::testTurnContextWithoutSkillsPort()
{
    FakeProjectContext projectPort(sampleProject(), QString());
    FakeLinkedFiles filesPort({Session::LinkedFile{"main.cpp", "int main() {}"}});

    Session::TurnContextRequest request;
    request.message = "/review this";
    request.basePrompt = "base";

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, nullptr, filesPort).build(request);

    QVERIFY(context.alwaysOnSkills.isEmpty());
    QVERIFY(context.skillsCatalog.isEmpty());
    QVERIFY(context.invokedSkills.isEmpty());
    QVERIFY(context.linkedFiles.isEmpty());
    QVERIFY(filesPort.requestedPaths().isEmpty());
}

void QodeAssistTest::testLinkedFilesHeaderSurvivesUnreadablePaths()
{
    FakeProjectContext projectPort({}, QString());
    FakeLinkedFiles filesPort({});

    Session::TurnContextRequest request;
    request.basePrompt = "base";
    request.linkedFilePaths = QList<QString>{"/src/ignored.cpp"};

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, nullptr, filesPort).build(request);

    QVERIFY(context.linkedFiles.isEmpty());
    QCOMPARE(context.linkedFilePaths, (QList<QString>{"/src/ignored.cpp"}));
    QCOMPARE(
        Session::renderSystemPrompt(context),
        QString("base\n# No active project in IDE\n\nLinked files for reference:\n"));
}

void QodeAssistTest::testSystemPromptRenderingWithProject()
{
    Session::TurnContext context;
    context.basePrompt = "You are helpful.";
    context.rolePrompt = "Be terse.";
    context.project = sampleProject();
    context.projectRules = "Use tabs.";
    context.alwaysOnSkills = "# Always on";
    context.skillsCatalog = "# Catalog";
    context.invokedSkills = {Session::InvokedSkill{"review", "Review body"}};
    context.linkedFilePaths = QList<QString>{"/src/main.cpp"};
    context.linkedFiles = {Session::LinkedFile{"main.cpp", "int main() {}"}};

    const QString expected
        = "You are helpful."
          "\n\nBe terse."
          "\n# Active project: QodeAssist"
          "\n# Project source root: /src/QodeAssist"
          "\n#   All new source files, headers, QML and CMake edits MUST be created or modified "
          "under this directory. Use absolute paths rooted here, or project-relative paths."
          "\n# Build output directory (compiler artifacts only — do NOT create or edit source "
          "files here): /src/QodeAssist/build"
          "\n# Project Rules\n\nUse tabs."
          "\n\n# Always on"
          "\n\n# Catalog"
          "\n\n# Invoked Skill: review\n\nReview body"
          "\n\nLinked files for reference:\n"
          "\nFile: main.cpp\nContent:\nint main() {}\n";

    QCOMPARE(Session::renderSystemPrompt(context), expected);
}

void QodeAssistTest::testSystemPromptRenderingWithoutProject()
{
    Session::TurnContext context;
    context.basePrompt = "You are helpful.";

    QCOMPARE(
        Session::renderSystemPrompt(context),
        QString("You are helpful.\n# No active project in IDE"));
}

} // namespace QodeAssist
