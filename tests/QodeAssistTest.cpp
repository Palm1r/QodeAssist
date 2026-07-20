// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "QodeAssistTest.hpp"

#include "ChatView/ChatHistoryBridge.hpp"
#include "ChatView/ChatModel.hpp"
#include "FakeAcpAgent.hpp"
#include "acp/AcpChatBackend.hpp"
#include "acp/AgentCatalog.hpp"
#include "acp/AgentCatalogStore.hpp"
#include "acp/AgentLaunch.hpp"
#include "acp/AgentRegistryParser.hpp"
#include "acp/AgentSpawn.hpp"

#include <LLMQore/AcpClient.hpp>
#include "completion/CodeHandler.hpp"
#include "completion/LLMSuggestion.hpp"
#include "llmcore/ContextData.hpp"
#include "providers/ClaudeCacheControl.hpp"
#include "session/HistoryProjection.hpp"
#include "session/HistorySerializer.hpp"
#include "session/Session.hpp"
#include "session/TurnContextBuilder.hpp"

#include <QDir>
#include <QFile>
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

class FakeChatBackend : public Session::ChatBackend
{
public:
    void sendTurn(const Session::TurnRequest &request) override
    {
        ++m_turnCount;
        m_lastOptions = request.options;
        m_lastUserBlocks = request.userBlocks;
        m_historySizeAtSend = request.history ? request.history->size() : -1;
        m_lastContext = request.context;
    }

    void cancel() override { ++m_cancelCount; }

    void script(const QList<Session::SessionEvent> &events)
    {
        for (const Session::SessionEvent &scripted : events)
            emit sessionEvent(scripted);
    }

    int turnCount() const { return m_turnCount; }
    int cancelCount() const { return m_cancelCount; }
    qsizetype historySizeAtSend() const { return m_historySizeAtSend; }
    const QList<Session::ContentBlock> &lastUserBlocks() const { return m_lastUserBlocks; }
    const std::optional<Session::TurnContext> &lastContext() const { return m_lastContext; }
    Session::TurnOptions lastOptions() const { return m_lastOptions; }

private:
    int m_turnCount = 0;
    int m_cancelCount = 0;
    qsizetype m_historySizeAtSend = -1;
    QList<Session::ContentBlock> m_lastUserBlocks;
    std::optional<Session::TurnContext> m_lastContext;
    Session::TurnOptions m_lastOptions;
};

QList<Session::SessionEvent> scriptedTurn(const QString &turnId)
{
    return {
        Session::TurnStarted{turnId},
        Session::ThinkingReceived{turnId, "weighing options", "sig-1", false},
        Session::TextDelta{turnId, "let me "},
        Session::TextDelta{turnId, "check"},
        Session::ToolCallStarted{turnId, "t1", "read_file", QJsonObject{{"path", "main.cpp"}}, false},
        Session::ToolCallCompleted{turnId, "t1", "read_file", "int main() {}"},
        Session::TextDelta{turnId, "here is the answer"},
        Session::UsageReported{turnId, Session::Usage{120, 40, 80, 12}},
        Session::TurnCompleted{turnId}};
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
    Session::Session session;
    Chat::ChatHistoryBridge bridge(&session, &model);

    session.setHistory(historyWithoutFileEdits());

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

    const QList<Session::MessageRow> rows = Session::projectToRows(history);

    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).content, QString("first\n[Signature: sig-a...]"));
    QCOMPARE(rows.at(1).content, QString("second\n[Signature: sig-b...]"));
    QCOMPARE(Session::buildFromRows(rows), history);
}

void QodeAssistTest::testSessionAppendsUserTurnBeforeSending()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn(
        {Session::TextBlock{"explain this"}, Session::AttachmentBlock{"notes.txt", "notes_ab.txt"}},
        std::nullopt,
        Session::TurnOptions{true, false});

    QCOMPARE(backend.turnCount(), 1);
    QCOMPARE(backend.historySizeAtSend(), qsizetype(1));
    QCOMPARE(backend.lastUserBlocks().size(), 2);
    QCOMPARE(backend.lastOptions(), (Session::TurnOptions{true, false}));
    QVERIFY(!backend.lastContext().has_value());

    QCOMPARE(session.history().size(), 1);
    QCOMPARE(session.history().at(0).role, Session::MessageRole::User);
    QCOMPARE(session.rows().size(), 1);
    QCOMPARE(session.rows().at(0).content, QString("explain this"));
    QCOMPARE(session.rows().at(0).attachments.size(), 1);
}

void QodeAssistTest::testSessionStreamsTextThinkingAndTools()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"explain this"}}, std::nullopt, {});
    backend.script(scriptedTurn("r1"));

    QCOMPARE(session.history().size(), 2);

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.role, Session::MessageRole::Assistant);
    QCOMPARE(assistant.id, QString("r1"));
    QCOMPARE(assistant.usage, (Session::Usage{120, 40, 80, 12}));
    QCOMPARE(assistant.blocks.size(), 4);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{Session::ThinkingBlock{"weighing options", "sig-1", false}}));
    QCOMPARE(assistant.blocks.at(1), (Session::ContentBlock{Session::TextBlock{"let me check"}}));
    QCOMPARE(
        assistant.blocks.at(2),
        (Session::ContentBlock{Session::ToolCallBlock{
            "t1", "read_file", QJsonObject{{"path", "main.cpp"}}, "int main() {}"}}));
    QCOMPARE(
        assistant.blocks.at(3), (Session::ContentBlock{Session::TextBlock{"here is the answer"}}));
}

void QodeAssistTest::testSessionTrimsStreamedText()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::TextDelta{"r1", "\n\n"},
         Session::TextDelta{"r1", "  here is"},
         Session::TextDelta{"r1", " the answer  \n"}});

    QCOMPARE(session.history().at(1).blocks.size(), 1);
    QCOMPARE(
        session.history().at(1).blocks.at(0),
        (Session::ContentBlock{Session::TextBlock{"here is the answer"}}));
}

void QodeAssistTest::testSessionAbandoningTheTurnCancelsTheBackend()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, Session::TextDelta{"r1", "partial"}});

    const int cancelsBeforeTruncate = backend.cancelCount();
    session.truncateRows(0);
    QCOMPARE(backend.cancelCount(), cancelsBeforeTruncate + 1);

    const int cancelsBeforeLoad = backend.cancelCount();
    session.setHistory(Session::ConversationHistory{});
    QCOMPARE(backend.cancelCount(), cancelsBeforeLoad + 1);
}

void QodeAssistTest::testSessionProjectionMatchesHistory()
{
    Chat::ChatModel model;
    FakeChatBackend backend;
    Session::Session session;
    Chat::ChatHistoryBridge bridge(&session, &model);
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"explain this"}}, std::nullopt, {});
    backend.script(scriptedTurn("r1"));

    const QList<Session::MessageRow> expected = Session::projectToRows(session.history());
    QCOMPARE(session.rows(), expected);
    QCOMPARE(model.rowCount(), int(expected.size()));

    for (int row = 0; row < expected.size(); ++row) {
        QCOMPARE(
            model.data(model.index(row), Chat::ChatModel::Content).toString(),
            expected.at(row).content);
    }

    QCOMPARE(model.sessionPromptTokens(), 120);
    QCOMPARE(model.sessionCompletionTokens(), 40);
}

void QodeAssistTest::testSessionAccumulatesStreamedThinking()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ThinkingReceived{"r1", "step", QString(), false},
         Session::ThinkingReceived{"r1", "step one", QString(), false},
         Session::ThinkingReceived{"r1", "step one and two", QString(), false},
         Session::TextDelta{"r1", "done"}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{Session::ThinkingBlock{"step one and two", QString(), false}}));
    QCOMPARE(session.rows().size(), 3);
}

void QodeAssistTest::testSessionKeepsThinkingAfterToolContinuation()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ThinkingReceived{"r1", "first thought", "sig-a", false},
         Session::ToolCallStarted{"r1", "t1", "read_file", QJsonObject{}, false},
         Session::ToolCallCompleted{"r1", "t1", "read_file", "contents"},
         Session::ThinkingReceived{"r1", "second thought", "sig-b", false},
         Session::TextDelta{"r1", "done"}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 4);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{Session::ThinkingBlock{"first thought", "sig-a", false}}));
    QCOMPARE(
        assistant.blocks.at(2),
        (Session::ContentBlock{Session::ThinkingBlock{"second thought", "sig-b", false}}));
}

void QodeAssistTest::testSessionDropsPreToolTextWhenAsked()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::TextDelta{"r1", "leaked tool preamble"},
         Session::ToolCallStarted{"r1", "t1", "read_file", QJsonObject{}, true},
         Session::ToolCallCompleted{"r1", "t1", "read_file", "contents"},
         Session::TextDelta{"r1", "the answer"}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);
    QCOMPARE(
        assistant.blocks.at(0),
        (Session::ContentBlock{
            Session::ToolCallBlock{"t1", "read_file", QJsonObject{}, "contents"}}));
    QCOMPARE(assistant.blocks.at(1), (Session::ContentBlock{Session::TextBlock{"the answer"}}));
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void QodeAssistTest::testSessionToolResultAppendsFileEditRow()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    const QString result
        = "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\",\"file\":\"main.cpp\",\"status\":\"pending\"}";

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallStarted{"r1", "t1", "edit_file", QJsonObject{}, false},
         Session::ToolCallCompleted{"r1", "t1", "edit_file", result}});

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 2);
    QCOMPARE(assistant.blocks.at(1), (Session::ContentBlock{Session::FileEditBlock{"e1", result}}));

    QVERIFY(session.updateFileEditStatus("e1", "applied", "Successfully applied"));

    const Session::MessageRow &editRow = session.rows().last();
    QCOMPARE(editRow.kind, Session::RowKind::FileEdit);
    QVERIFY(editRow.content.contains("\"status\":\"applied\""));
    QVERIFY(editRow.content.contains("\"status_message\":\"Successfully applied\""));
}

void QodeAssistTest::testSessionIgnoresEchoedFileEditMarker()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    const QString echoed
        = "Pre-compression history (/src): 1 matching message(s)\n\n[#3 assistant]\n"
          "QODEASSIST_FILE_EDIT:{\"edit_id\":\"e1\",\"file\":\"main.cpp\"}";

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallStarted{"r1", "t1", "read_original_history", QJsonObject{}, false},
         Session::ToolCallCompleted{"r1", "t1", "read_original_history", echoed}});

    QCOMPARE(session.history().at(1).blocks.size(), 1);
    QCOMPARE(session.rows().size(), 2);
}

void QodeAssistTest::testSessionTruncationKeepsBlockOrder()
{
    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks
        = {Session::TextBlock{"look at this"},
           Session::AttachmentBlock{"notes.txt", "notes_ab.txt"},
           Session::ImageBlock{"shot.png", "shot_cd.png", "image/png"}};

    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks = {Session::TextBlock{"sure"}};

    Session::ConversationHistory history;
    history.append(user);
    history.append(assistant);

    Session::Session session;
    session.setHistory(history);
    session.truncateRows(1);

    QCOMPARE(session.history().size(), 1);
    QCOMPARE(session.history().at(0).blocks, user.blocks);
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
}

void QodeAssistTest::testHistorySnapshotReportsUnreadableBlocks()
{
    QJsonObject block;
    block["type"] = "from_a_newer_build";

    QJsonObject message;
    message["role"] = "assistant";
    message["blocks"] = QJsonArray{block, QJsonObject{{"type", "text"}, {"text", "kept"}}};

    QJsonObject root;
    root["version"] = Session::HistorySerializer::currentVersion();
    root["messages"] = QJsonArray{message};

    int dropped = -1;
    const auto history = Session::HistorySerializer::fromJson(root, &dropped);

    QVERIFY(history.has_value());
    QCOMPARE(dropped, 1);
    QCOMPARE(history->at(0).blocks.size(), 1);
}

void QodeAssistTest::testSessionUsageLandsOnTheTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ThinkingReceived{"r1", "hmm", "sig", false},
         Session::TextDelta{"r1", "done"},
         Session::UsageReported{"r1", Session::Usage{10, 5, 2, 1}},
         Session::TurnCompleted{"r1"}});

    QCOMPARE(session.history().at(1).usage, (Session::Usage{10, 5, 2, 1}));
    QCOMPARE(session.rows().at(1).usage, (Session::Usage{10, 5, 2, 1}));
    QCOMPARE(session.rows().at(2).usage, Session::Usage{});
}

void QodeAssistTest::testSessionIgnoresEventsFromOtherTurns()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::TextDelta{"r1", "mine"},
         Session::TextDelta{"other", "not mine"},
         Session::ToolCallStarted{"other", "t9", "read_file", QJsonObject{}, false}});

    QCOMPARE(session.history().size(), 2);
    QCOMPARE(session.history().at(1).blocks.size(), 1);
    QCOMPARE(
        session.history().at(1).blocks.at(0), (Session::ContentBlock{Session::TextBlock{"mine"}}));
}

void QodeAssistTest::testSessionReportsFailureWithoutStartedTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    QString reported;
    QObject::connect(
        &session, &Session::Session::turnFailed, &session, [&reported](const QString &error) {
            reported = error;
        });

    backend.script({Session::TurnFailed{QString(), "no provider configured"}});

    QCOMPARE(reported, QString("no provider configured"));
}

void QodeAssistTest::testSessionIgnoresFailureFromAbandonedTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    int reported = 0;
    QObject::connect(&session, &Session::Session::turnFailed, &session, [&reported](const QString &) {
        ++reported;
    });

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, Session::TextDelta{"r1", "partial"}});
    session.truncateRows(0);

    backend.script({Session::TurnFailed{"r1", "stale timeout"}});

    QCOMPARE(reported, 0);
}

void QodeAssistTest::testSessionTruncateRowsRewritesHistory()
{
    Chat::ChatModel model;
    FakeChatBackend backend;
    Session::Session session;
    Chat::ChatHistoryBridge bridge(&session, &model);
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"explain this"}}, std::nullopt, {});
    backend.script(scriptedTurn("r1"));

    const qsizetype before = session.rows().size();
    QVERIFY(before > 1);

    session.truncateRows(1);

    QCOMPARE(session.rows().size(), qsizetype(1));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(session.history().size(), 1);
    QCOMPARE(session.history().at(0).role, Session::MessageRole::User);
    QCOMPARE(session.rows(), Session::projectToRows(session.history()));
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

void QodeAssistTest::testAgentRegistryParsesEveryDistributionKind()
{
    const QByteArray payload = R"({"version":"1.0.0","agents":[
        {"id":"npx-agent","name":"Npx Agent","version":"1.2.3","description":"An agent",
         "authors":["Ada","Grace"],"license":"MIT","repository":"https://example.com/repo",
         "website":"https://example.com","icon":"https://example.com/icon.svg",
         "distribution":{"npx":{"package":"pkg@1.2.3","args":["--acp"],"env":{"KEY":"VALUE"}}}},
        {"id":"uvx-agent","name":"Uvx Agent",
         "distribution":{"uvx":{"package":"pypkg==1.0","args":["-x"]}}},
        {"id":"binary-agent","name":"Binary Agent","distribution":{"binary":{
            "darwin-aarch64":{"archive":"https://example.com/a.tar.gz","sha256":"abc",
                              "cmd":"./dist/agent","args":["acp"]}}}},
        {"id":"command-agent","name":"Command Agent",
         "distribution":{"command":{"cmd":"/usr/local/bin/agent","args":["acp"]}}}
    ]})";

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        payload, Acp::AgentSource::LiveRegistry, "registry");

    QVERIFY(result.warnings.isEmpty());
    QCOMPARE(result.agents.size(), 4);

    const Acp::AgentDefinition &npx = result.agents.at(0);
    QCOMPARE(npx.id, QString("npx-agent"));
    QCOMPARE(npx.name, QString("Npx Agent"));
    QCOMPARE(npx.version, QString("1.2.3"));
    QCOMPARE(npx.description, QString("An agent"));
    QCOMPARE(npx.authors, (QStringList{"Ada", "Grace"}));
    QCOMPARE(npx.license, QString("MIT"));
    QCOMPARE(npx.repository, QString("https://example.com/repo"));
    QCOMPARE(npx.source, Acp::AgentSource::LiveRegistry);
    QCOMPARE(npx.origin, QString("registry"));
    QCOMPARE(npx.distribution.kind, Acp::AgentDistributionKind::Npx);
    QCOMPARE(npx.distribution.package, QString("pkg@1.2.3"));
    QCOMPARE(npx.distribution.args, (QStringList{"--acp"}));
    QCOMPARE(npx.distribution.env.size(), 1);
    QCOMPARE(npx.distribution.env.at(0).name, QString("KEY"));
    QCOMPARE(npx.distribution.env.at(0).value, QString("VALUE"));
    QVERIFY(npx.isLaunchable());

    const Acp::AgentDefinition &uvx = result.agents.at(1);
    QCOMPARE(uvx.distribution.kind, Acp::AgentDistributionKind::Uvx);
    QCOMPARE(uvx.distribution.package, QString("pypkg==1.0"));
    QVERIFY(uvx.isLaunchable());

    const Acp::AgentDefinition &binary = result.agents.at(2);
    QCOMPARE(binary.distribution.kind, Acp::AgentDistributionKind::Binary);
    QCOMPARE(binary.distribution.binaryTargets.size(), 1);
    QCOMPARE(binary.distribution.binaryTargets.at(0).platform, QString("darwin-aarch64"));
    QCOMPARE(binary.distribution.binaryTargets.at(0).cmd, QString("./dist/agent"));
    QCOMPARE(binary.distribution.binaryTargets.at(0).sha256, QString("abc"));
    QVERIFY(!binary.isLaunchable());

    const Acp::AgentDefinition &command = result.agents.at(3);
    QCOMPARE(command.distribution.kind, Acp::AgentDistributionKind::Command);
    QCOMPARE(command.distribution.command, QString("/usr/local/bin/agent"));
    QVERIFY(command.isLaunchable());
}

void QodeAssistTest::testAgentRegistryReportsUnusableEntries()
{
    const QByteArray payload = R"({"agents":[
        {"name":"No Id","distribution":{"npx":{"package":"p"}}},
        {"id":"no-distribution","name":"No Distribution"},
        {"id":"usable","name":"Usable","distribution":{"npx":{"package":"p"}}}
    ]})";

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        payload, Acp::AgentSource::LiveRegistry, "registry");

    QCOMPARE(result.agents.size(), 2);
    QCOMPARE(result.warnings.size(), 2);
    QVERIFY(result.warnings.at(0).contains("entry 0"));
    QVERIFY(result.warnings.at(1).contains("no-distribution"));

    QCOMPARE(result.agents.at(0).id, QString("no-distribution"));
    QVERIFY(!result.agents.at(0).isLaunchable());
    QVERIFY(result.agents.at(1).isLaunchable());
}

void QodeAssistTest::testAgentRegistryParsesSingleAgentUserFile()
{
    const QByteArray payload
        = R"({"id":"local","distribution":{"command":{"cmd":"/opt/local-agent"}}})";

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        payload, Acp::AgentSource::UserFile, "local.json");

    QCOMPARE(result.agents.size(), 1);
    QCOMPARE(result.agents.at(0).id, QString("local"));
    QCOMPARE(result.agents.at(0).name, QString("local"));
    QCOMPARE(result.agents.at(0).source, Acp::AgentSource::UserFile);
    QVERIFY(result.agents.at(0).isLaunchable());
}

void QodeAssistTest::testAgentCatalogAppliesMergePriority()
{
    const QByteArray bundled = R"({"agents":[
        {"id":"alpha","name":"Alpha Bundled","distribution":{"npx":{"package":"alpha@1"}}},
        {"id":"beta","name":"Beta","distribution":{"npx":{"package":"beta@1"}}}
    ]})";
    const QByteArray live = R"({"agents":[
        {"id":"alpha","name":"Alpha Registry","distribution":{"npx":{"package":"alpha@2"}}},
        {"id":"gamma","name":"Gamma","distribution":{"uvx":{"package":"gamma"}}}
    ]})";
    const QByteArray user
        = R"({"id":"alpha","name":"Alpha User","distribution":{"command":{"cmd":"/opt/alpha"}}})";

    Acp::AgentCatalog catalog;
    catalog.setLayer(
        Acp::AgentSource::BundledSnapshot,
        Acp::AgentRegistryParser::parse(bundled, Acp::AgentSource::BundledSnapshot, "bundled")
            .agents);
    catalog.setLayer(
        Acp::AgentSource::LiveRegistry,
        Acp::AgentRegistryParser::parse(live, Acp::AgentSource::LiveRegistry, "registry").agents);
    catalog.setLayer(
        Acp::AgentSource::UserFile,
        Acp::AgentRegistryParser::parse(user, Acp::AgentSource::UserFile, "alpha.json").agents);

    QCOMPARE(catalog.size(), 3);

    QStringList names;
    for (const Acp::AgentDefinition &agent : catalog.agents())
        names.append(agent.name);
    QCOMPARE(names, (QStringList{"Alpha User", "Beta", "Gamma"}));

    auto alpha = catalog.agent("alpha");
    QVERIFY(alpha.has_value());
    QCOMPARE(alpha->source, Acp::AgentSource::UserFile);
    QCOMPARE(alpha->distribution.kind, Acp::AgentDistributionKind::Command);

    catalog.setLayer(Acp::AgentSource::UserFile, {});
    alpha = catalog.agent("alpha");
    QVERIFY(alpha.has_value());
    QCOMPARE(alpha->name, QString("Alpha Registry"));
    QCOMPARE(alpha->distribution.package, QString("alpha@2"));

    catalog.setLayer(Acp::AgentSource::LiveRegistry, {});
    alpha = catalog.agent("alpha");
    QVERIFY(alpha.has_value());
    QCOMPARE(alpha->name, QString("Alpha Bundled"));
}

void QodeAssistTest::testAgentCatalogUserFileMakesBinaryAgentLaunchable()
{
    const QByteArray live = R"({"agents":[{"id":"cursor","name":"Cursor",
        "distribution":{"binary":{"darwin-aarch64":{"archive":"https://example.com/a.tar.gz",
                                                    "cmd":"./cursor-agent","args":["acp"]}}}}]})";
    const QByteArray user = R"({"id":"cursor","name":"Cursor",
        "distribution":{"command":{"cmd":"/usr/local/bin/cursor-agent","args":["acp"]}}})";

    Acp::AgentCatalog catalog;
    catalog.setLayer(
        Acp::AgentSource::LiveRegistry,
        Acp::AgentRegistryParser::parse(live, Acp::AgentSource::LiveRegistry, "registry").agents);
    QVERIFY(catalog.launchableAgents().isEmpty());

    catalog.setLayer(
        Acp::AgentSource::UserFile,
        Acp::AgentRegistryParser::parse(user, Acp::AgentSource::UserFile, "cursor.json").agents);

    QCOMPARE(catalog.size(), 1);
    QCOMPARE(catalog.launchableAgents().size(), 1);
    QCOMPARE(catalog.agent("cursor")->distribution.command, QString("/usr/local/bin/cursor-agent"));
}

void QodeAssistTest::testAgentLaunchConfigUsesRunnerConventions()
{
    Acp::AgentDefinition npx;
    npx.id = "npx-agent";
    npx.distribution.kind = Acp::AgentDistributionKind::Npx;
    npx.distribution.package = "pkg@1.0";
    npx.distribution.args = QStringList{"--acp"};
    npx.distribution.env = {{"KEY", "VALUE"}};

    const auto npxConfig = Acp::agentLaunchConfig(npx, "/tmp/project");
    QVERIFY(npxConfig.has_value());
    QCOMPARE(npxConfig->cwd, QString("/tmp/project"));
    QCOMPARE(npxConfig->command, QString("npx"));
    QCOMPARE(npxConfig->args, (QStringList{"-y", "pkg@1.0", "--acp"}));
    QCOMPARE(npxConfig->env.size(), 1);
    QCOMPARE(npxConfig->env.at(0).name, QString("KEY"));

    Acp::AgentDefinition uvx;
    uvx.id = "uvx-agent";
    uvx.distribution.kind = Acp::AgentDistributionKind::Uvx;
    uvx.distribution.package = "pypkg==1.0";
    uvx.distribution.args = QStringList{"-x"};

    const auto uvxConfig = Acp::agentLaunchConfig(uvx, QString());
    QVERIFY(uvxConfig.has_value());
    QCOMPARE(uvxConfig->command, QString("uvx"));
    QCOMPARE(uvxConfig->args, (QStringList{"pypkg==1.0", "-x"}));

    Acp::AgentDefinition command;
    command.id = "command-agent";
    command.distribution.kind = Acp::AgentDistributionKind::Command;
    command.distribution.command = "/opt/agent";
    command.distribution.args = QStringList{"acp"};

    const auto commandConfig = Acp::agentLaunchConfig(command, QString());
    QVERIFY(commandConfig.has_value());
    QCOMPARE(commandConfig->command, QString("/opt/agent"));
    QCOMPARE(commandConfig->args, (QStringList{"acp"}));

    Acp::AgentDefinition binary;
    binary.id = "binary-agent";
    binary.distribution.kind = Acp::AgentDistributionKind::Binary;
    QVERIFY(!Acp::agentLaunchConfig(binary, QString()).has_value());
}

void QodeAssistTest::testAgentSearchPathsSplitting()
{
    const QString separator(QDir::listSeparator());

    QCOMPARE(Acp::splitSearchPaths(QString()), QStringList());
    QCOMPARE(
        Acp::splitSearchPaths(" /opt/homebrew/bin " + separator + separator + "/usr/local/bin"),
        (QStringList{"/opt/homebrew/bin", "/usr/local/bin"}));
}

void QodeAssistTest::testAgentExtraSearchPathsReachTheChildEnvironment()
{
    Acp::AgentDefinition agent;
    agent.id = "npx-agent";
    agent.distribution.kind = Acp::AgentDistributionKind::Npx;
    agent.distribution.package = "pkg";

    auto config = Acp::agentLaunchConfig(agent, QString());
    QVERIFY(config.has_value());

    Acp::applyExtraSearchPaths(*config, {});
    QVERIFY(config->env.isEmpty());
    QCOMPARE(config->command, QString("npx"));

    Acp::applyExtraSearchPaths(*config, QStringList{"/no/such/qodeassist/dir"});
    QCOMPARE(config->command, QString("npx"));
    QCOMPARE(config->env.size(), 1);
    QCOMPARE(config->env.at(0).name, QString("PATH"));
    QVERIFY(config->env.at(0).value.startsWith("/no/such/qodeassist/dir"));

    Acp::AgentDefinition withOwnPath;
    withOwnPath.id = "own-path";
    withOwnPath.distribution.kind = Acp::AgentDistributionKind::Npx;
    withOwnPath.distribution.package = "pkg";
    withOwnPath.distribution.env = {{"PATH", "/agent/own"}};

    auto ownPathConfig = Acp::agentLaunchConfig(withOwnPath, QString());
    QVERIFY(ownPathConfig.has_value());
    Acp::applyExtraSearchPaths(*ownPathConfig, QStringList{"/extra"});

    QCOMPARE(ownPathConfig->env.size(), 2);
    QCOMPARE(ownPathConfig->env.at(0).name, QString("PATH"));
    QCOMPARE(ownPathConfig->env.at(1).value, QString("/agent/own"));
}

void QodeAssistTest::testAgentForwardedVariablesReachTheChildEnvironment()
{
    QCOMPARE(
        Acp::splitVariableNames(" FOO, BAR\tBAZ "), (QStringList{"FOO", "BAR", "BAZ"}));
    QCOMPARE(Acp::splitVariableNames(QString()), QStringList());

    qputenv("QODEASSIST_TEST_TOKEN", "from-environment");

    Acp::AgentDefinition agent;
    agent.id = "npx-agent";
    agent.distribution.kind = Acp::AgentDistributionKind::Npx;
    agent.distribution.package = "pkg";
    agent.distribution.env = {{"QODEASSIST_TEST_TOKEN", "from-definition"}};

    auto config = Acp::agentLaunchConfig(agent, QString());
    QVERIFY(config.has_value());

    Acp::applyForwardedEnvironment(*config, {});
    QCOMPARE(config->env.size(), 1);

    Acp::applyForwardedEnvironment(*config, QStringList{"QODEASSIST_TEST_TOKEN"});

    QCOMPARE(config->env.size(), 2);
    QCOMPARE(config->env.at(0).name, QString("QODEASSIST_TEST_TOKEN"));
    QCOMPARE(config->env.at(0).value, QString("from-environment"));
    QCOMPARE(config->env.at(1).value, QString("from-definition"));

    qunsetenv("QODEASSIST_TEST_TOKEN");
}

void QodeAssistTest::testBundledAgentSnapshotParses()
{
    QFile file(Acp::AgentCatalogStore::bundledSnapshotPath());
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        file.readAll(), Acp::AgentSource::BundledSnapshot, "bundled snapshot");

    QVERIFY(result.agents.size() > 10);
    QVERIFY2(result.warnings.isEmpty(), qPrintable(result.warnings.join("; ")));

    Acp::AgentCatalog catalog;
    catalog.setLayer(Acp::AgentSource::BundledSnapshot, result.agents);
    QVERIFY(!catalog.launchableAgents().isEmpty());
}

namespace {

Acp::AgentDefinition fakeAgentDefinition()
{
    Acp::AgentDefinition agent;
    agent.id = "fake-agent";
    agent.name = "Fake Agent";
    agent.distribution.kind = Acp::AgentDistributionKind::Command;
    agent.distribution.command = "/bin/true";
    return agent;
}

class AcpBackendFixture
{
public:
    AcpBackendFixture()
    {
        backend.setClientFactory(
            [this](const Acp::AgentDefinition &, const QString &cwd, QObject *parent) {
                requestedCwd = cwd;
                agent = new FakeAcpAgent(parent);
                configure(agent);
                return Acp::AgentProcess{
                    new LLMQore::Acp::AcpClient(
                        agent,
                        {QStringLiteral("QodeAssistTest"), QStringLiteral("1.0"), QString()},
                        parent),
                    QStringLiteral("fake")};
            });

        QObject::connect(
            &backend,
            &Session::ChatBackend::sessionEvent,
            &backend,
            [this](const Session::SessionEvent &event) { events.append(event); });

        backend.bindAgent(fakeAgentDefinition());
    }

    void send(const QString &text)
    {
        Session::TurnRequest request;
        request.userBlocks = {Session::TextBlock{text}};
        backend.sendTurn(request);
    }

    template<typename T>
    QList<T> eventsOfType() const
    {
        QList<T> result;
        for (const Session::SessionEvent &event : events) {
            if (const auto *typed = std::get_if<T>(&event))
                result.append(*typed);
        }
        return result;
    }

    std::function<void(FakeAcpAgent *)> configure = [](FakeAcpAgent *) {};
    Acp::AcpChatBackend backend;
    FakeAcpAgent *agent = nullptr;
    QString requestedCwd;
    QList<Session::SessionEvent> events;
};

} // namespace

void QodeAssistTest::testAcpBackendStreamsTextAndThinking()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setThoughtChunks({"pondering"});
        agent->setReplyChunks({"Hello", " world"});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnStarted>().size(), 1);
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);

    const auto deltas = fixture.eventsOfType<Session::TextDelta>();
    QCOMPARE(deltas.size(), 2);
    QCOMPARE(deltas.at(0).text, QString("Hello"));
    QCOMPARE(deltas.at(1).text, QString(" world"));

    const auto thinking = fixture.eventsOfType<Session::ThinkingReceived>();
    QCOMPARE(thinking.size(), 1);
    QCOMPARE(thinking.at(0).text, QString("pondering"));

    const QString turnId = fixture.eventsOfType<Session::TurnStarted>().at(0).turnId;
    QVERIFY(!turnId.isEmpty());
    QCOMPARE(deltas.at(0).turnId, turnId);
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().at(0).turnId, turnId);

    QCOMPARE(fixture.agent->prompts().size(), 1);
    QCOMPARE(fixture.agent->prompts().at(0).size(), 1);
    QCOMPARE(
        fixture.agent->prompts().at(0).at(0).toObject().value("text").toString(), QString("hi"));
}

void QodeAssistTest::testAcpBackendStartsSessionInWorkingDirectory()
{
    AcpBackendFixture fixture;
    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.requestedCwd, Acp::agentWorkingDirectory());
    QCOMPARE(fixture.agent->newSessionCwd(), Acp::agentWorkingDirectory());
    QVERIFY(!fixture.agent->newSessionCwd().isEmpty());
    QCOMPARE(fixture.agent->clientInfo().value("name").toString(), QString("QodeAssistTest"));
}

void QodeAssistTest::testAcpBackendAuthenticatesWhenTheAgentAsksForIt()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setRequireAuthentication(true); };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
    QCOMPARE(fixture.agent->authMethodUsed(), QString("login"));
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 2);
}

void QodeAssistTest::testAcpBackendReportsPromptFailure()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setPromptFailure("agent exploded"); };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
    QVERIFY(fixture.eventsOfType<Session::TurnFailed>().at(0).error.contains("agent exploded"));
    QCOMPARE(
        fixture.eventsOfType<Session::TurnFailed>().at(0).turnId,
        fixture.eventsOfType<Session::TurnStarted>().at(0).turnId);
}

void QodeAssistTest::testAcpBackendCancelInterruptsTheTurn()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setHangOnPrompt(true);
        agent->setReplyChunks({"partial"});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TextDelta>().size(), 1);

    fixture.backend.cancel();

    QTRY_VERIFY(fixture.agent->wasCancelled());
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
}

} // namespace QodeAssist
