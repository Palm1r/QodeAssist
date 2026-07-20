// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "QodeAssistTest.hpp"

#include "ChatView/ChatHistoryBridge.hpp"
#include "ChatView/ChatModel.hpp"
#include "ChatView/ChatSerializer.hpp"
#include "FakeAcpAgent.hpp"
#include "acp/AcpChatBackend.hpp"
#include "acp/AgentBinding.hpp"
#include "acp/AgentKnowledgeService.hpp"
#include "mcp/AgentKnowledgeServer.hpp"
#include "tools/BuildProjectTool.hpp"
#include "tools/CreateNewFileTool.hpp"
#include "tools/EditFileTool.hpp"
#include "tools/ExecuteTerminalCommandTool.hpp"
#include "tools/EditorStateTools.hpp"
#include "tools/GetIssuesListTool.hpp"
#include "tools/ReadFileTool.hpp"

#include <LLMQore/ToolsManager.hpp>
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
#include "session/FencedText.hpp"
#include "session/HistoryProjection.hpp"
#include "session/HistorySerializer.hpp"
#include "session/Session.hpp"
#include "session/TurnContextBuilder.hpp"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QUrl>

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
    Session::TurnOptions lastOptions() const { return m_lastOptions; }

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
    Session::TurnOptions m_lastOptions;
};

QList<Session::PermissionOption> editPermissionOptions()
{
    return {
        Session::PermissionOption{"opt-once", "Yes", "allow_once"},
        Session::PermissionOption{"opt-always", "Yes, and don't ask again", "allow_always"},
        Session::PermissionOption{"opt-no", "No", "reject_once"}};
}

Session::PermissionRequested editPermissionRequest(
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

Session::PermissionBlock permissionRowAt(const Session::Session &session, int index)
{
    return Session::decodePermissionBlock(session.rows().at(index).content)
        .value_or(Session::PermissionBlock{});
}

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

    QList<Session::LinkedFile> resolvePaths(const QList<QString> &paths) const override
    {
        m_resolvedOnly = true;
        m_requestedPaths = paths;

        QList<Session::LinkedFile> files;
        for (const Session::LinkedFile &file : m_files)
            files.append(Session::LinkedFile{file.fileName, {}, file.path});
        return files;
    }

    QList<QString> requestedPaths() const { return m_requestedPaths; }
    bool resolvedWithoutContent() const { return m_resolvedOnly; }

private:
    QList<Session::LinkedFile> m_files;
    mutable QList<QString> m_requestedPaths;
    mutable bool m_resolvedOnly = false;
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

void QodeAssistTest::testSessionUpsertsAgentToolCalls()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit main.cpp",
             .kind = "edit",
             .status = "pending"},
         Session::ToolCallUpdated{
             .turnId = "r1",
             .toolId = "call-1",
             .name = "Edit main.cpp",
             .kind = "edit",
             .status = "completed",
             .result = "done",
             .details = QJsonObject{{"locations", QJsonArray{QJsonObject{{"path", "/p/main.cpp"}}}}}}});

    QCOMPARE(session.history().size(), 2);

    const Session::Message &assistant = session.history().at(1);
    QCOMPARE(assistant.blocks.size(), 1);

    const auto *tool = std::get_if<Session::ToolCallBlock>(&assistant.blocks.at(0));
    QVERIFY(tool);
    QCOMPARE(tool->id, QString("call-1"));
    QCOMPARE(tool->name, QString("Edit main.cpp"));
    QCOMPARE(tool->kind, QString("edit"));
    QCOMPARE(tool->status, QString("completed"));
    QCOMPARE(tool->result, QString("done"));
    QVERIFY(tool->details.contains("locations"));

    QCOMPARE(session.rows().size(), 2);
    QCOMPARE(session.rows().at(1).kind, Session::RowKind::AgentTool);
    QCOMPARE(session.rows().at(1).toolStatus, QString("completed"));
    QCOMPARE(session.rows().at(1).toolKind, QString("edit"));
    QCOMPARE(session.rows().at(1).toolDetails, tool->details);
    QVERIFY(tool->fromAgent);
}

void QodeAssistTest::testSessionKeepsOnePlanPerTurn()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"fix it"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::PlanUpdated{
             .turnId = "r1",
             .entries = {Session::PlanEntry{"Read", "high", "in_progress"},
                         Session::PlanEntry{"Fix", "medium", "pending"}}},
         Session::TextDelta{"r1", "working"},
         Session::PlanUpdated{
             .turnId = "r1",
             .entries = {Session::PlanEntry{"Read", "high", "completed"},
                         Session::PlanEntry{"Fix", "medium", "in_progress"}}}});

    const Session::Message &assistant = session.history().at(1);

    qsizetype planBlocks = 0;
    const Session::PlanBlock *plan = nullptr;
    for (const Session::ContentBlock &block : assistant.blocks) {
        if (const auto *candidate = std::get_if<Session::PlanBlock>(&block)) {
            ++planBlocks;
            plan = candidate;
        }
    }

    QCOMPARE(planBlocks, qsizetype(1));
    QVERIFY(plan);
    QCOMPARE(plan->entries.size(), 2);
    QCOMPARE(plan->entries.at(0).status, QString("completed"));
    QCOMPARE(plan->entries.at(1).status, QString("in_progress"));

    qsizetype planRows = 0;
    for (const Session::MessageRow &row : session.rows()) {
        if (row.kind == Session::RowKind::Plan)
            ++planRows;
    }
    QCOMPARE(planRows, qsizetype(1));
}

void QodeAssistTest::testAgentActivityKeepsStreamedTextIntact()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1", .toolId = "call-1", .name = "Read main.cpp", .status = "pending"},
         Session::TextDelta{"r1", "Reading the file"},
         Session::ToolCallUpdated{
             .turnId = "r1", .toolId = "call-1", .name = "Read main.cpp", .status = "completed"},
         Session::TextDelta{"r1", " and it is done."}});

    const Session::Message &assistant = session.history().at(1);

    QStringList streamed;
    for (const Session::ContentBlock &block : assistant.blocks) {
        if (const auto *text = std::get_if<Session::TextBlock>(&block))
            streamed.append(text->text);
    }

    QCOMPARE(streamed, QStringList{"Reading the file and it is done."});
}

void QodeAssistTest::testAgentToolUpdateTouchesOnlyItsOwnRow()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::ToolCallUpdated{
             .turnId = "r1", .toolId = "call-1", .name = "Read a", .status = "pending"},
         Session::ToolCallUpdated{
             .turnId = "r1", .toolId = "call-2", .name = "Read b", .status = "pending"}});

    QList<int> updatedRows;
    QObject::connect(
        &session,
        &Session::Session::rowUpdated,
        &session,
        [&updatedRows](int index, const Session::MessageRow &) { updatedRows.append(index); });

    backend.script(
        {Session::ToolCallUpdated{
            .turnId = "r1", .toolId = "call-1", .name = "Read a", .status = "completed"}});

    QCOMPARE(updatedRows, QList<int>{1});
    QCOMPARE(session.rows().at(1).toolStatus, QString("completed"));
    QCOMPARE(session.rows().at(2).toolStatus, QString("pending"));
}

void QodeAssistTest::testUsageSkipsRowsThatCannotShowIt()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"go"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         Session::PlanUpdated{
             .turnId = "r1", .entries = {Session::PlanEntry{"Read", "high", "in_progress"}}},
         Session::TextDelta{"r1", "here is the answer"},
         Session::UsageReported{"r1", Session::Usage{120, 40, 80, 12}}});

    const Session::MessageRow &planRow = session.rows().at(1);
    const Session::MessageRow &textRow = session.rows().at(2);

    QCOMPARE(planRow.kind, Session::RowKind::Plan);
    QVERIFY(planRow.usage.isEmpty());

    QCOMPARE(textRow.kind, Session::RowKind::Assistant);
    QCOMPARE(textRow.usage, (Session::Usage{120, 40, 80, 12}));
}

void QodeAssistTest::testAgentActivityBlocksSurviveReload()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks
        = {Session::PlanBlock{
               {Session::PlanEntry{"Read", "high", "completed"},
                Session::PlanEntry{"Fix", "medium", "in_progress"}}},
           Session::ToolCallBlock{
               "call-1",
               "Edit main.cpp",
               QJsonObject{},
               "done",
               "edit",
               "completed",
               QJsonObject{{"diffs", QJsonArray{QJsonObject{{"path", "/p/main.cpp"}}}}},
               true}};

    Session::ConversationHistory history;
    history.append(assistant);

    const auto reloaded = Session::HistorySerializer::fromJson(
        Session::HistorySerializer::toJson(history));
    QVERIFY(reloaded.has_value());
    QCOMPARE(*reloaded, history);

    const QList<Session::MessageRow> rows = Session::projectToRows(history);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).kind, Session::RowKind::Plan);
    QCOMPARE(rows.at(1).kind, Session::RowKind::AgentTool);
    QVERIFY(Session::isTranscriptOnlyRow(rows.at(0).kind));
    QVERIFY(Session::isTranscriptOnlyRow(rows.at(1).kind));
    QCOMPARE(Session::buildFromRows(rows), history);
}

void QodeAssistTest::testSessionRendersPermissionRequestWithItsOptions()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    QCOMPARE(session.rows().size(), 2);
    QCOMPARE(session.rows().at(1).kind, Session::RowKind::Permission);
    QCOMPARE(session.rows().at(1).id, QString("p1"));

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.requestId, QString("p1"));
    QCOMPARE(block.toolCallId, QString("tool-p1"));
    QCOMPARE(block.title, QString("Edit main.cpp"));
    QCOMPARE(block.toolKind, QString("edit"));
    QCOMPARE(block.options, editPermissionOptions());
    QCOMPARE(block.status, Session::PermissionStatus::Pending);
    QVERIFY(!block.automatic);
    QVERIFY(session.isPermissionPending("p1"));

    QVERIFY(backend.permissionAnswers().isEmpty());
}

void QodeAssistTest::testSessionForwardsPermissionAnswerToTheBackend()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("p1", "opt-no");

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(backend.permissionAnswers().at(0).first, QString("p1"));
    QCOMPARE(backend.permissionAnswers().at(0).second, QString("opt-no"));

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.status, Session::PermissionStatus::Answered);
    QCOMPARE(block.selectedOptionId, QString("opt-no"));
    QVERIFY(!block.automatic);
    QVERIFY(!session.isPermissionPending("p1"));
}

void QodeAssistTest::testSessionAllowAlwaysSuppressesRepeatPromptsOfTheSameKind()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit them"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("p1", "opt-always");

    backend.script({editPermissionRequest("r1", "p2")});

    QCOMPARE(backend.permissionAnswers().size(), 2);
    QCOMPARE(backend.permissionAnswers().at(1).first, QString("p2"));
    QCOMPARE(backend.permissionAnswers().at(1).second, QString("opt-once"));

    const Session::PermissionBlock block = permissionRowAt(session, 2);
    QCOMPARE(block.requestId, QString("p2"));
    QCOMPARE(block.status, Session::PermissionStatus::Answered);
    QCOMPARE(block.selectedOptionId, QString("opt-once"));
    QVERIFY(block.automatic);
    QVERIFY(!session.isPermissionPending("p2"));
}

void QodeAssistTest::testSessionAllowAlwaysDoesNotCoverOtherToolKinds()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1", "edit")});

    session.respondPermission("p1", "opt-always");

    backend.script({editPermissionRequest("r1", "p2", "execute")});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
    QVERIFY(session.isPermissionPending("p2"));
}

void QodeAssistTest::testSessionNeverAutoAnswersUnclassifiedTools()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1", QString())});

    session.respondPermission("p1", "opt-always");

    backend.script({editPermissionRequest("r1", "p2", QString())});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
}

void QodeAssistTest::testSessionIgnoresAnswersToUnknownPermissionRequests()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("nonexistent", "opt-once");
    session.respondPermission("p1", "no-such-option");
    session.respondPermission("p1", "opt-once");
    session.respondPermission("p1", "opt-no");

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(backend.permissionAnswers().at(0).second, QString("opt-once"));
    QCOMPARE(permissionRowAt(session, 1).selectedOptionId, QString("opt-once"));
}

void QodeAssistTest::testSessionMarksUnansweredPermissionsCancelled()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    backend.script(
        {Session::PermissionResolved{.turnId = "r1", .requestId = "p1", .cancelled = true},
         Session::TurnFailed{"r1", "the agent went away"}});

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.status, Session::PermissionStatus::Cancelled);
    QVERIFY(block.selectedOptionId.isEmpty());
    QVERIFY(!session.isPermissionPending("p1"));

    session.respondPermission("p1", "opt-once");
    QVERIFY(backend.permissionAnswers().isEmpty());
}

void QodeAssistTest::testSessionRejectAlwaysSuppressesRepeatPromptsOfTheSameKind()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    Session::PermissionRequested first = editPermissionRequest("r1", "p1");
    first.options.append(
        Session::PermissionOption{"opt-never", "No, and don't ask again", "reject_always"});

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, first});

    session.respondPermission("p1", "opt-never");

    Session::PermissionRequested second = editPermissionRequest("r1", "p2");
    second.options.append(
        Session::PermissionOption{"opt-never", "No, and don't ask again", "reject_always"});
    backend.script({second});

    QCOMPARE(backend.permissionAnswers().size(), 2);
    QCOMPARE(backend.permissionAnswers().at(1).second, QString("opt-no"));

    const Session::PermissionBlock block = permissionRowAt(session, 2);
    QCOMPARE(block.status, Session::PermissionStatus::Answered);
    QCOMPARE(block.selectedOptionId, QString("opt-no"));
    QVERIFY(block.automatic);
}

void QodeAssistTest::testSessionAutoAnswerFallsBackToTheAlwaysOption()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});
    session.respondPermission("p1", "opt-always");

    Session::PermissionRequested withoutOnce = editPermissionRequest("r1", "p2");
    withoutOnce.options = {
        Session::PermissionOption{"opt-always", "Yes, always", "allow_always"},
        Session::PermissionOption{"opt-no", "No", "reject_once"}};
    backend.script({withoutOnce});

    QCOMPARE(backend.permissionAnswers().size(), 2);
    QCOMPARE(backend.permissionAnswers().at(1).second, QString("opt-always"));
}

void QodeAssistTest::testSessionDoesNotAutoAnswerWithoutAMatchingOption()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});
    session.respondPermission("p1", "opt-always");

    Session::PermissionRequested rejectOnly = editPermissionRequest("r1", "p2");
    rejectOnly.options = {Session::PermissionOption{"opt-no", "No", "reject_once"}};
    backend.script({rejectOnly});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
    QVERIFY(session.isPermissionPending("p2"));
}

void QodeAssistTest::testSessionAllowAlwaysDoesNotSurviveAnotherConversation()
{
    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"work"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});
    session.respondPermission("p1", "opt-always");
    QCOMPARE(backend.permissionAnswers().size(), 1);

    session.setHistory(Session::ConversationHistory{});

    session.sendTurn({Session::TextBlock{"work again"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r2"}, editPermissionRequest("r2", "p2")});

    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 1).status, Session::PermissionStatus::Pending);
}

void QodeAssistTest::testSessionAnswersTheLiveRequestWhenAnOldIdIsReused()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "old";
    assistant.blocks
        = {Session::PermissionBlock{
            .requestId = "p1",
            .toolCallId = "tool-old",
            .title = "Run make",
            .toolKind = "execute",
            .options = editPermissionOptions(),
            .status = Session::PermissionStatus::Cancelled}};

    Session::ConversationHistory history;
    history.append(assistant);

    FakeChatBackend backend;
    Session::Session session;
    session.setBackend(&backend);
    session.setHistory(history);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1", "edit")});

    session.respondPermission("p1", "opt-always");

    QCOMPARE(backend.permissionAnswers().size(), 1);

    const Session::PermissionBlock stale = permissionRowAt(session, 0);
    QCOMPARE(stale.status, Session::PermissionStatus::Cancelled);
    QVERIFY(stale.selectedOptionId.isEmpty());

    const Session::PermissionBlock live = permissionRowAt(session, 2);
    QCOMPARE(live.status, Session::PermissionStatus::Answered);
    QCOMPARE(live.selectedOptionId, QString("opt-always"));

    backend.script({editPermissionRequest("r1", "p3", "edit")});
    QCOMPARE(backend.permissionAnswers().size(), 2);

    backend.script({editPermissionRequest("r1", "p4", "execute")});
    QCOMPARE(backend.permissionAnswers().size(), 2);
}

void QodeAssistTest::testSessionCancelsPermissionsTheBackendNeverResolved()
{
    FakeChatBackend backend;
    backend.setAutoResolvePermissions(false);

    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script(
        {Session::TurnStarted{"r1"},
         editPermissionRequest("r1", "p1"),
         Session::TurnCompleted{"r1"}});

    QCOMPARE(permissionRowAt(session, 1).status, Session::PermissionStatus::Cancelled);
    QVERIFY(!session.isPermissionPending("p1"));
}

void QodeAssistTest::testSessionCancelsPermissionsTheBackendRefused()
{
    FakeChatBackend backend;
    backend.setAcceptPermissionAnswers(false);

    Session::Session session;
    session.setBackend(&backend);

    session.sendTurn({Session::TextBlock{"edit it"}}, std::nullopt, {});
    backend.script({Session::TurnStarted{"r1"}, editPermissionRequest("r1", "p1")});

    session.respondPermission("p1", "opt-always");

    const Session::PermissionBlock block = permissionRowAt(session, 1);
    QCOMPARE(block.status, Session::PermissionStatus::Cancelled);
    QVERIFY(block.selectedOptionId.isEmpty());
    QVERIFY(!session.isPermissionPending("p1"));

    backend.script({editPermissionRequest("r1", "p2")});
    QCOMPARE(backend.permissionAnswers().size(), 1);
    QCOMPARE(permissionRowAt(session, 2).status, Session::PermissionStatus::Pending);
}

void QodeAssistTest::testPermissionBlockReopensAsUnanswerable()
{
    Session::Message assistant;
    assistant.role = Session::MessageRole::Assistant;
    assistant.id = "r1";
    assistant.blocks
        = {Session::PermissionBlock{
               .requestId = "p1",
               .toolCallId = "tool-1",
               .title = "Edit main.cpp",
               .toolKind = "edit",
               .options = editPermissionOptions(),
               .status = Session::PermissionStatus::Answered,
               .selectedOptionId = "opt-once",
               .automatic = false},
           Session::PermissionBlock{
               .requestId = "p2",
               .toolCallId = "tool-2",
               .title = "Run make",
               .toolKind = "execute",
               .options = editPermissionOptions(),
               .status = Session::PermissionStatus::Pending}};

    Session::ConversationHistory history;
    history.append(assistant);

    const QJsonObject json = Session::HistorySerializer::toJson(history);
    const auto reloaded = Session::HistorySerializer::fromJson(json);
    QVERIFY(reloaded.has_value());
    QCOMPARE(reloaded->size(), 1);
    QCOMPARE(reloaded->at(0).blocks.size(), 2);

    const auto *answered = std::get_if<Session::PermissionBlock>(&reloaded->at(0).blocks.at(0));
    QVERIFY(answered);
    QCOMPARE(*answered, std::get<Session::PermissionBlock>(assistant.blocks.at(0)));

    const auto *stale = std::get_if<Session::PermissionBlock>(&reloaded->at(0).blocks.at(1));
    QVERIFY(stale);
    QCOMPARE(stale->status, Session::PermissionStatus::Cancelled);
    QCOMPARE(stale->options, editPermissionOptions());

    const QList<Session::MessageRow> rows = Session::projectToRows(*reloaded);
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).kind, Session::RowKind::Permission);

    const Session::ConversationHistory rebuilt = Session::buildFromRows(rows);
    QCOMPARE(rebuilt.size(), 1);
    QCOMPARE(rebuilt.at(0).blocks, reloaded->at(0).blocks);
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

void QodeAssistTest::testTurnContextSkipsWhatTheBackendDoesNotNeed()
{
    FakeProjectContext projectPort(sampleProject(), "Always use tabs.");
    FakeSkillsContext skillsPort(
        "# Always on", "# Skills catalog", {Session::InvokedSkill{"review", "Review body"}});
    FakeLinkedFiles filesPort({Session::LinkedFile{"main.cpp", "int main() {}", "/src/main.cpp"}});

    Session::TurnContextRequest request;
    request.message = "please /review this";
    request.linkedFilePaths = QList<QString>{"/src/main.cpp"};
    request.needs = Session::TurnContextNeeds{false, false};

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, &skillsPort, filesPort).build(request);

    QVERIFY(context.projectRules.isEmpty());
    QVERIFY(context.alwaysOnSkills.isEmpty());
    QVERIFY(context.skillsCatalog.isEmpty());
    QVERIFY(context.invokedSkills.isEmpty());

    QVERIFY(filesPort.resolvedWithoutContent());
    QCOMPARE(context.linkedFiles.size(), 1);
    QCOMPARE(context.linkedFiles.at(0).fileName, QString("main.cpp"));
    QCOMPARE(context.linkedFiles.at(0).path, QString("/src/main.cpp"));
    QVERIFY(context.linkedFiles.at(0).content.isEmpty());
}

void QodeAssistTest::testFencedFileBlockOutgrowsBackticksInContent()
{
    const QString plain = Session::fencedFileBlock("notes.txt", "hello");
    QCOMPARE(plain, QString("File: notes.txt\n```\nhello\n```"));

    const QString fenced = Session::fencedFileBlock("readme.md", "text\n```\ncode\n```\nmore");
    QVERIFY(fenced.startsWith("File: readme.md\n````\n"));
    QVERIFY(fenced.endsWith("\n````"));
    QVERIFY(fenced.contains("```\ncode\n```"));

    const QString nested = Session::fencedFileBlock("deep.md", "`````");
    QVERIFY(nested.contains("``````"));
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

        backend.setStoredContentLoader([this](const QString &, const QString &storedPath) {
            return storage.value(storedPath);
        });

        backend.bindAgent(fakeAgentDefinition());
    }

    void send(const QString &text)
    {
        Session::TurnRequest request;
        request.userBlocks = {Session::TextBlock{text}};
        backend.sendTurn(request);
    }

    void sendBlocks(
        const QList<Session::ContentBlock> &blocks,
        const std::optional<Session::TurnContext> &context = std::nullopt)
    {
        Session::TurnRequest request;
        request.userBlocks = blocks;
        request.context = context;
        backend.sendTurn(request);
    }

    QList<QJsonObject> promptBlocksOfType(const QString &type) const
    {
        QList<QJsonObject> matching;
        if (!agent)
            return matching;

        const QList<QJsonArray> sent = agent->prompts();
        if (sent.isEmpty())
            return matching;

        for (const QJsonValue &block : sent.constLast()) {
            if (block.toObject().value("type").toString() == type)
                matching.append(block.toObject());
        }
        return matching;
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
    QHash<QString, QByteArray> storage;
    FakeAcpAgent *agent = nullptr;
    QString requestedCwd;
    QList<Session::SessionEvent> events;
    Acp::AcpChatBackend backend;
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

void QodeAssistTest::testAcpBackendSendsAttachmentsAsEmbeddedResources()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };
    fixture.storage.insert("notes_ab12.txt", QByteArray("remember the milk"));

    fixture.sendBlocks(
        {Session::TextBlock{"summarise this"},
         Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto resources = fixture.promptBlocksOfType("resource");
    QCOMPARE(resources.size(), 1);

    const QJsonObject resource = resources.at(0).value("resource").toObject();
    QCOMPARE(resource.value("text").toString(), QString("remember the milk"));
    QCOMPARE(resource.value("mimeType").toString(), QString("text/plain"));
    QVERIFY(resource.value("uri").toString().endsWith("notes.txt"));

    QCOMPARE(fixture.promptBlocksOfType("text").size(), 1);
}

void QodeAssistTest::testAcpBackendInlinesAttachmentsWithoutEmbeddedContextSupport()
{
    AcpBackendFixture fixture;
    fixture.storage.insert("notes_ab12.txt", QByteArray("remember the milk"));

    fixture.sendBlocks(
        {Session::TextBlock{"summarise this"},
         Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.promptBlocksOfType("resource").size(), 0);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);

    const QString inlined = texts.at(1).value("text").toString();
    QVERIFY(inlined.contains("notes.txt"));
    QVERIFY(inlined.contains("remember the milk"));
}

void QodeAssistTest::testAcpBackendReportsAttachmentsItCannotLoad()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };

    fixture.sendBlocks(
        {Session::TextBlock{"summarise this"},
         Session::AttachmentBlock{"notes.txt", "missing.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.promptBlocksOfType("resource").size(), 0);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);
    QVERIFY(texts.at(1).value("text").toString().contains("notes.txt"));
}

void QodeAssistTest::testAcpBackendSendsImagesWhenTheAgentAcceptsThem()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsImages(true); };
    fixture.storage.insert("shot_cd34.png", QByteArray("\x89PNG-bytes"));

    fixture.sendBlocks(
        {Session::TextBlock{"what is this"},
         Session::ImageBlock{"shot.png", "shot_cd34.png", "image/png"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto images = fixture.promptBlocksOfType("image");
    QCOMPARE(images.size(), 1);
    QCOMPARE(images.at(0).value("mimeType").toString(), QString("image/png"));
    QCOMPARE(
        images.at(0).value("data").toString(),
        QString::fromLatin1(QByteArray("\x89PNG-bytes").toBase64()));
}

void QodeAssistTest::testAcpBackendNamesImagesTheAgentCannotAccept()
{
    AcpBackendFixture fixture;
    fixture.storage.insert("shot_cd34.png", QByteArray("\x89PNG-bytes"));

    fixture.sendBlocks(
        {Session::TextBlock{"what is this"},
         Session::ImageBlock{"shot.png", "shot_cd34.png", "image/png"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.promptBlocksOfType("image").size(), 0);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);
    QVERIFY(texts.at(1).value("text").toString().contains("shot.png"));
}

void QodeAssistTest::testAcpBackendSendsLinkedFilesAsResourceLinks()
{
    AcpBackendFixture fixture;

    Session::TurnContext context;
    context.linkedFilePaths = {"/project/main.cpp", "/project/secret.key"};
    context.linkedFiles = {Session::LinkedFile{"main.cpp", "int main() {}", "/project/main.cpp"}};

    fixture.sendBlocks({Session::TextBlock{"review this"}}, context);

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto links = fixture.promptBlocksOfType("resource_link");
    QCOMPARE(links.size(), 1);
    QCOMPARE(links.at(0).value("name").toString(), QString("main.cpp"));
    QCOMPARE(links.at(0).value("uri").toString(), QString("file:///project/main.cpp"));
}

void QodeAssistTest::testAcpBackendPercentEncodesResourceLinkUris()
{
    AcpBackendFixture fixture;

    Session::TurnContext context;
    context.linkedFiles
        = {Session::LinkedFile{"my notes.cpp", QString(), "/project/dir/my notes.cpp"},
           Session::LinkedFile{"файл.cpp", QString(), "/project/файл.cpp"}};

    fixture.sendBlocks({Session::TextBlock{"review this"}}, context);

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto links = fixture.promptBlocksOfType("resource_link");
    QCOMPARE(links.size(), 2);
    QCOMPARE(
        links.at(0).value("uri").toString(), QString("file:///project/dir/my%20notes.cpp"));

    const QString unicodeUri = links.at(1).value("uri").toString();
    QVERIFY(!unicodeUri.contains(QString::fromUtf8("файл")));
    QCOMPARE(QUrl(unicodeUri).toLocalFile(), QString::fromUtf8("/project/файл.cpp"));
}

void QodeAssistTest::testAcpBackendFencesAttachmentsThatContainFences()
{
    AcpBackendFixture fixture;
    fixture.storage.insert("readme_ab12.md", QByteArray("intro\n```\ncode\n```\noutro"));

    fixture.sendBlocks(
        {Session::TextBlock{"read it"}, Session::AttachmentBlock{"readme.md", "readme_ab12.md"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 2);

    const QString inlined = texts.at(1).value("text").toString();
    QVERIFY(inlined.startsWith("File: readme.md\n````\n"));
    QVERIFY(inlined.endsWith("\n````"));
    QVERIFY(inlined.contains("```\ncode\n```"));
}

void QodeAssistTest::testAcpBackendTruncatesOversizedAttachments()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };

    const QByteArray huge(700 * 1024, 'x');
    fixture.storage.insert("dump_ab12.log", huge);

    fixture.sendBlocks(
        {Session::TextBlock{"summarise"}, Session::AttachmentBlock{"dump.log", "dump_ab12.log"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto resources = fixture.promptBlocksOfType("resource");
    QCOMPARE(resources.size(), 1);

    const QString text = resources.at(0).value("resource").toObject().value("text").toString();
    QVERIFY(text.size() < huge.size());
    QVERIFY(text.contains("truncated by QodeAssist"));
}

void QodeAssistTest::testAcpBackendRejectsATurnOfUnlinkableFilesOnly()
{
    AcpBackendFixture fixture;

    Session::TurnContext context;
    context.linkedFilePaths = {"/project/main.cpp"};
    context.linkedFiles = {Session::LinkedFile{"main.cpp", "int main() {}", QString()}};

    fixture.sendBlocks({Session::TextBlock{""}}, context);

    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);
    QCOMPARE(fixture.eventsOfType<Session::TurnStarted>().size(), 0);
    QVERIFY(fixture.agent == nullptr);
}

void QodeAssistTest::testAcpBackendSendsAttachmentsWithoutAnyMessageText()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsEmbeddedContext(true); };
    fixture.storage.insert("notes_ab12.txt", QByteArray("remember the milk"));

    fixture.sendBlocks(
        {Session::TextBlock{""}, Session::AttachmentBlock{"notes.txt", "notes_ab12.txt"}});

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
    QCOMPARE(fixture.promptBlocksOfType("resource").size(), 1);
}

namespace {

QJsonObject writeToolCall()
{
    return QJsonObject{
        {"toolCallId", QStringLiteral("call-1")},
        {"title", QStringLiteral("Write main.cpp")},
        {"kind", QStringLiteral("edit")},
        {"status", QStringLiteral("pending")}};
}

QJsonArray writePermissionOptions()
{
    return QJsonArray{
        QJsonObject{
            {"optionId", QStringLiteral("allow")},
            {"name", QStringLiteral("Allow")},
            {"kind", QStringLiteral("allow_once")}},
        QJsonObject{
            {"optionId", QStringLiteral("deny")},
            {"name", QStringLiteral("Deny")},
            {"kind", QStringLiteral("reject_once")}}};
}

} // namespace

void QodeAssistTest::testAcpBackendRaisesAgentPermissionRequests()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const Session::PermissionRequested request
        = fixture.eventsOfType<Session::PermissionRequested>().at(0);
    QCOMPARE(request.turnId, fixture.eventsOfType<Session::TurnStarted>().at(0).turnId);
    QVERIFY(!request.requestId.isEmpty());
    QCOMPARE(request.toolCallId, QString("call-1"));
    QCOMPARE(request.title, QString("Write main.cpp"));
    QCOMPARE(request.toolKind, QString("edit"));
    QCOMPARE(request.options.size(), 2);
    QCOMPARE(request.options.at(0), (Session::PermissionOption{"allow", "Allow", "allow_once"}));
    QCOMPARE(request.options.at(1), (Session::PermissionOption{"deny", "Deny", "reject_once"}));

    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
    QVERIFY(fixture.agent->permissionOutcomes().isEmpty());
}

void QodeAssistTest::testAcpBackendSendsThePermissionAnswerToTheAgent()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const QString requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;
    fixture.backend.respondPermission(requestId, "allow");

    QTRY_COMPARE(fixture.agent->permissionOutcomes().size(), 1);
    QCOMPARE(
        fixture.agent->permissionOutcomes().at(0).value("outcome").toString(), QString("selected"));
    QCOMPARE(fixture.agent->permissionOutcomes().at(0).value("optionId").toString(), QString("allow"));

    const auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QCOMPARE(resolutions.at(0).requestId, requestId);
    QCOMPARE(resolutions.at(0).optionId, QString("allow"));
    QVERIFY(!resolutions.at(0).cancelled);

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
}

void QodeAssistTest::testAcpBackendCancelsOutstandingPermissionRequests()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const QString requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;
    fixture.backend.cancel();

    QTRY_COMPARE(fixture.agent->permissionOutcomes().size(), 1);
    QCOMPARE(
        fixture.agent->permissionOutcomes().at(0).value("outcome").toString(), QString("cancelled"));

    const auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QCOMPARE(resolutions.at(0).requestId, requestId);
    QVERIFY(resolutions.at(0).cancelled);
    QVERIFY(resolutions.at(0).optionId.isEmpty());

    QTRY_VERIFY(fixture.agent->wasCancelled());
    QCOMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 0);
}

namespace {

class FakeKnowledgeService : public Acp::AgentKnowledgeService
{
public:
    QString start() override
    {
        ++startCount;
        running = true;
        return url;
    }

    void stop() override
    {
        ++stopCount;
        running = false;
    }

    QString serverName() const override { return QStringLiteral("qodeassist"); }

    QString url = QStringLiteral("http://127.0.0.1:54321/mcp");
    int startCount = 0;
    int stopCount = 0;
    bool running = false;
};

} // namespace

namespace {

class FakeGatedTool : public ::LLMQore::BaseTool
{
public:
    FakeGatedTool(QString toolId, ::LLMQore::ToolSafety safety, QObject *parent = nullptr)
        : ::LLMQore::BaseTool(parent)
        , m_id(std::move(toolId))
        , m_safety(safety)
    {}

    QString id() const override { return m_id; }
    QString displayName() const override { return m_id; }
    QString description() const override { return m_id; }
    QJsonObject parametersSchema() const override { return {}; }
    ::LLMQore::ToolSafety safety() const override { return m_safety; }

    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &) override
    {
        ++executions;
        QPromise<LLMQore::ToolResult> promise;
        promise.start();
        promise.addResult(LLMQore::ToolResult::text(QStringLiteral("ran ") + m_id));
        promise.finish();
        return promise.future();
    }

    int executions = 0;

private:
    QString m_id;
    ::LLMQore::ToolSafety m_safety;
};

} // namespace

void QodeAssistTest::testAcpBackendSeedsAHandoverSummaryOnce()
{
    AcpBackendFixture fixture;

    fixture.backend.setHandoverSummary("Earlier the user asked about parsing.");

    fixture.send("carry on");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const QList<QJsonObject> first = fixture.promptBlocksOfType("text");
    QCOMPARE(first.size(), 2);
    QVERIFY(first.at(0).value("text").toString().contains("Earlier the user asked about parsing."));
    QVERIFY(first.at(0).value("text").toString().contains("summary"));
    QCOMPARE(first.at(1).value("text").toString(), QString("carry on"));

    fixture.send("and now this");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 2);

    const QList<QJsonObject> second = fixture.promptBlocksOfType("text");
    QCOMPARE(second.size(), 1);
    QCOMPARE(second.at(0).value("text").toString(), QString("and now this"));
}

void QodeAssistTest::testAcpBackendDropsAHandoverSummaryAfterACancelledTurn()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setHangOnPrompt(true); };

    fixture.backend.setHandoverSummary("Earlier context.");

    fixture.send("carry on");
    QTRY_COMPARE(fixture.agent->prompts().size(), 1);

    QVERIFY(
        fixture.agent->prompts().at(0).at(0).toObject().value("text").toString().contains(
            "Earlier context."));

    fixture.backend.cancel();

    fixture.configure = [](FakeAcpAgent *) {};
    fixture.send("actually this");

    QTRY_COMPARE(fixture.agent->prompts().size(), 2);

    const QJsonArray second = fixture.agent->prompts().at(1);
    QCOMPARE(second.size(), 1);
    QCOMPARE(second.at(0).toObject().value("text").toString(), QString("actually this"));
}

void QodeAssistTest::testAcpBackendDropsAHandoverSummaryWithTheConversation()
{
    AcpBackendFixture fixture;

    fixture.backend.setHandoverSummary("stale summary");
    fixture.backend.clearToolSession(QString());

    fixture.send("fresh start");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const QList<QJsonObject> texts = fixture.promptBlocksOfType("text");
    QCOMPARE(texts.size(), 1);
    QCOMPARE(texts.at(0).value("text").toString(), QString("fresh start"));
}

void QodeAssistTest::testToolSafetyDefaultsToMutating()
{
    FakeGatedTool undeclared("undeclared", ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(undeclared.safety(), ::LLMQore::ToolSafety::Mutating);

    QCOMPARE(Tools::ReadFileTool().safety(), ::LLMQore::ToolSafety::ReadOnly);
    QCOMPARE(Tools::GetIssuesListTool().safety(), ::LLMQore::ToolSafety::ReadOnly);
    QCOMPARE(Tools::ListOpenEditorsTool().safety(), ::LLMQore::ToolSafety::ReadOnly);

    QCOMPARE(Tools::EditFileTool().safety(), ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(Tools::CreateNewFileTool().safety(), ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(Tools::ExecuteTerminalCommandTool().safety(), ::LLMQore::ToolSafety::Mutating);
    QCOMPARE(Tools::BuildProjectTool().safety(), ::LLMQore::ToolSafety::Mutating);
}

void QodeAssistTest::testToolsManagerWithoutAGateRunsImmediately()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *tool = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    manager.addTool(tool);

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});

    QTRY_COMPARE(tool->executions, 1);
}

void QodeAssistTest::testToolsManagerGateCanDeclineAToolCall()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *tool = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    manager.addTool(tool);

    QStringList gated;
    manager.setExecutionGate(
        [&gated](
            const QString &, const QString &, const QString &toolName, const QJsonObject &) {
            gated.append(toolName);
            QPromise<bool> promise;
            promise.start();
            promise.addResult(false);
            promise.finish();
            return promise.future();
        });

    QString resultText;
    QObject::connect(
        &manager,
        &::LLMQore::ToolsManager::toolExecutionResult,
        &manager,
        [&resultText](const QString &, const QString &, const QString &, const QString &result) {
            resultText = result;
        });

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});

    QTRY_VERIFY(!resultText.isEmpty());

    QCOMPARE(gated, QStringList{"edit_thing"});
    QCOMPARE(tool->executions, 0);
    QVERIFY2(resultText.contains("declined"), qPrintable(resultText));
}

void QodeAssistTest::testToolsManagerGateCanAllowAToolCall()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *tool = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    manager.addTool(tool);

    manager.setExecutionGate(
        [](const QString &, const QString &, const QString &, const QJsonObject &) {
            QPromise<bool> promise;
            promise.start();
            promise.addResult(true);
            promise.finish();
            return promise.future();
        });

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});

    QTRY_COMPARE(tool->executions, 1);
}

void QodeAssistTest::testToolsManagerGateResumesTheQueueAfterADenial()
{
    ::LLMQore::ToolsManager manager(::LLMQore::ToolSchemaFormat::OpenAI);
    auto *denied = new FakeGatedTool("edit_thing", ::LLMQore::ToolSafety::Mutating, &manager);
    auto *allowed = new FakeGatedTool("read_thing", ::LLMQore::ToolSafety::ReadOnly, &manager);
    manager.addTool(denied);
    manager.addTool(allowed);

    manager.setExecutionGate(
        [](const QString &, const QString &, const QString &toolName, const QJsonObject &) {
            QPromise<bool> promise;
            promise.start();
            promise.addResult(toolName != QLatin1String("edit_thing"));
            promise.finish();
            return promise.future();
        });

    bool finished = false;
    QObject::connect(
        &manager,
        &::LLMQore::ToolsManager::toolExecutionComplete,
        &manager,
        [&finished](const QString &, const QHash<QString, LLMQore::ToolResult> &) {
            finished = true;
        });

    manager.executeToolCall("req-1", "call-1", "edit_thing", QJsonObject{});
    manager.executeToolCall("req-1", "call-2", "read_thing", QJsonObject{});

    QTRY_VERIFY(finished);
    QCOMPARE(denied->executions, 0);
    QCOMPARE(allowed->executions, 1);
}

void QodeAssistTest::testKnowledgeServerExposesOnlyReadOnlyTools()
{
    const QStringList exposed = Mcp::AgentKnowledgeServer::toolIds();

    QCOMPARE(
        QSet<QString>(exposed.cbegin(), exposed.cend()),
        (QSet<QString>{
            "list_open_editors", "get_editor_selection", "get_project_model", "get_issues_list"}));

    const QStringList mutating{
        Tools::EditFileTool().id(),
        Tools::CreateNewFileTool().id(),
        Tools::ExecuteTerminalCommandTool().id(),
        Tools::BuildProjectTool().id()};

    for (const QString &tool : mutating)
        QVERIFY2(!exposed.contains(tool), qPrintable(tool));
}

void QodeAssistTest::testKnowledgeServerBindsLoopbackOnAnEphemeralPort()
{
    Mcp::AgentKnowledgeServer server;

    const QString url = server.start();
    QVERIFY(!url.isEmpty());
    QVERIFY(server.runningPort() != 0);

    const QUrl parsed(url);
    QVERIFY(QHostAddress(parsed.host()).isLoopback());
    QCOMPARE(parsed.port(), int(server.runningPort()));

    QVERIFY2(
        parsed.path().length() > QStringLiteral("/mcp/").length(),
        qPrintable(parsed.path()));

    QCOMPARE(server.start(), url);

    server.stop();
    QCOMPARE(server.runningPort(), quint16(0));
    server.stop();
}

void QodeAssistTest::testAcpBackendOffersKnowledgeToHttpCapableAgents()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(true); };

    fixture.send("what am I looking at");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(knowledge.startCount, 1);
    QVERIFY(knowledge.running);

    const QJsonArray offered = fixture.agent->offeredMcpServers();
    QCOMPARE(offered.size(), 1);
    QCOMPARE(offered.at(0).toObject().value("name").toString(), QString("qodeassist"));
    QCOMPARE(offered.at(0).toObject().value("type").toString(), QString("http"));
    QCOMPARE(offered.at(0).toObject().value("url").toString(), knowledge.url);
}

void QodeAssistTest::testAcpBackendWithholdsKnowledgeFromAgentsWithoutHttpMcp()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(false); };

    fixture.send("what am I looking at");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(knowledge.startCount, 0);
    QVERIFY(!knowledge.running);
    QVERIFY(fixture.agent->offeredMcpServers().isEmpty());
}

void QodeAssistTest::testAcpBackendStopsTheKnowledgeServerWithTheSession()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(true); };

    fixture.send("hello");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QVERIFY(knowledge.running);

    fixture.backend.clearToolSession(QString());

    QCOMPARE(knowledge.stopCount, 1);
    QVERIFY(!knowledge.running);
}

void QodeAssistTest::testAcpBackendSurvivesAKnowledgeServerThatWillNotStart()
{
    AcpBackendFixture fixture;
    FakeKnowledgeService knowledge;
    knowledge.url.clear();
    fixture.backend.setKnowledgeService(&knowledge);
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsHttpMcp(true); };

    fixture.send("hello");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(knowledge.startCount, 1);
    QVERIFY(fixture.agent->offeredMcpServers().isEmpty());
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
}

void QodeAssistTest::testAcpBackendResumesAPersistedSession()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(true); };

    QStringList startedSessions;
    QObject::connect(
        &fixture.backend,
        &Acp::AcpChatBackend::agentSessionStarted,
        &fixture.backend,
        [&startedSessions](const QString &sessionId) { startedSessions.append(sessionId); });

    fixture.backend.resumeSession("previous-session");
    QVERIFY(fixture.backend.isResumePending());

    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.agent->loadedSessionId(), QString("previous-session"));
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 0);
    QCOMPARE(fixture.backend.acpSessionId(), QString("previous-session"));
    QCOMPARE(startedSessions, QStringList{"previous-session"});
    QVERIFY(!fixture.backend.isResumePending());
    QCOMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 0);
}

void QodeAssistTest::testAcpBackendReportsAnUnresumableSession()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setSupportsLoadSession(true);
        agent->setLoadSessionError("session expired");
    };

    QStringList unavailable;
    QObject::connect(
        &fixture.backend,
        &Acp::AcpChatBackend::agentSessionUnavailable,
        &fixture.backend,
        [&unavailable](const QString &reason) { unavailable.append(reason); });

    fixture.backend.resumeSession("previous-session");
    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    QCOMPARE(unavailable.size(), 1);
    QVERIFY(unavailable.at(0).contains("session expired"));
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 0);
    QVERIFY(fixture.backend.acpSessionId().isEmpty());
    QVERIFY(!fixture.backend.isResumePending());
    QVERIFY(fixture.eventsOfType<Session::TurnFailed>().at(0).error.contains("read-only"));
}

void QodeAssistTest::testAcpBackendRefusesToResumeWithoutAgentSupport()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(false); };

    QStringList unavailable;
    QObject::connect(
        &fixture.backend,
        &Acp::AcpChatBackend::agentSessionUnavailable,
        &fixture.backend,
        [&unavailable](const QString &reason) { unavailable.append(reason); });

    fixture.backend.resumeSession("previous-session");
    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    QCOMPARE(unavailable.size(), 1);
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/load")), 0);
    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 0);
    QVERIFY(fixture.backend.acpSessionId().isEmpty());
}

void QodeAssistTest::testAcpBackendStartsFreshAfterAFailedResume()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setSupportsLoadSession(true);
        agent->setLoadSessionError("session expired");
    };

    fixture.backend.resumeSession("previous-session");
    fixture.send("carry on");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnFailed>().size(), 1);

    fixture.backend.startFreshSession();
    fixture.send("start over");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 1);
    QCOMPARE(fixture.backend.acpSessionId(), QString("fake-session"));
    QCOMPARE(fixture.agent->prompts().size(), 1);
    QCOMPARE(
        fixture.agent->prompts().at(0).at(0).toObject().value("text").toString(),
        QString("start over"));
}

void QodeAssistTest::testAgentBindingKeepsTheResumeTargetBeforeTheFirstTurn()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(true); };

    QVERIFY(fixture.backend.bindingSessionId().isEmpty());

    fixture.backend.resumeSession("sess-from-file");
    QCOMPARE(fixture.backend.bindingSessionId(), QString("sess-from-file"));
    QVERIFY(fixture.backend.acpSessionId().isEmpty());

    fixture.send("carry on");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    QCOMPARE(fixture.backend.acpSessionId(), QString("sess-from-file"));
    QCOMPARE(fixture.backend.bindingSessionId(), QString("sess-from-file"));
}

void QodeAssistTest::testAcpBackendCarriesTheLiveSessionIntoAResume()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSupportsLoadSession(true); };

    fixture.send("hello");
    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QCOMPARE(fixture.backend.acpSessionId(), QString("fake-session"));

    fixture.backend.setChatFilePath("/somewhere/else.json");
    QCOMPARE(fixture.backend.bindingSessionId(), QString("fake-session"));

    fixture.backend.clearToolSession(QString());
    QVERIFY(fixture.backend.bindingSessionId().isEmpty());
}

void QodeAssistTest::testAcpBackendEstablishesTheSessionOnlyOnce()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setHangOnNewSession(true); };

    fixture.send("first");
    QTRY_COMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 1);

    fixture.backend.cancel();
    fixture.send("second");
    QTest::qWait(50);

    QCOMPARE(fixture.agent->methods().count(QStringLiteral("session/new")), 1);
    QCOMPARE(fixture.agent->prompts().size(), 0);
}

void QodeAssistTest::testAgentBindingRejectsMalformedRecords()
{
    const auto rejected = [](const QJsonValue &value) {
        QString error;
        const Acp::AgentBinding binding = Acp::AgentBinding::fromJson(value, &error);
        return binding.isEmpty() && !error.isEmpty();
    };

    QVERIFY(rejected(QJsonValue(QStringLiteral("claude"))));
    QVERIFY(rejected(QJsonValue(42)));
    QVERIFY(rejected(QJsonObject{{"agentId", 7}}));
    QVERIFY(rejected(QJsonObject{{"agentId", "claude"}, {"sessionId", QJsonArray{}}}));
    QVERIFY(rejected(QJsonObject{{"sessionId", "orphan"}}));
    QVERIFY(rejected(QJsonObject{{"agentId", QString(4096, QChar('a'))}}));
    QVERIFY(rejected(QJsonObject{{"agentId", QStringLiteral("clau‮de")}}));

    QString error;
    QVERIFY(Acp::AgentBinding::fromJson(QJsonValue(), &error).isEmpty());
    QVERIFY(error.isEmpty());

    const Acp::AgentBinding good = Acp::AgentBinding::fromJson(
        QJsonObject{{"agentId", "claude"}, {"sessionId", "sess-1"}}, &error);
    QCOMPARE(good, (Acp::AgentBinding{"claude", "sess-1"}));
    QVERIFY(error.isEmpty());
}

void QodeAssistTest::testMalformedAgentBindingLeavesTheChatUnbound()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("broken_agent.json");

    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks = {Session::TextBlock{"hello"}};

    Session::ConversationHistory history;
    history.append(user);

    QVERIFY(Chat::ChatSerializer::saveToFile(history, Acp::AgentBinding{}, filePath).success);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    root["agent"] = QStringLiteral("claude");
    file.resize(0);
    file.write(QJsonDocument(root).toJson());
    file.close();

    Session::ConversationHistory reloaded;
    Acp::AgentBinding binding;
    const Chat::SerializationResult result
        = Chat::ChatSerializer::loadFromFile(reloaded, binding, filePath);

    QVERIFY(result.success);
    QVERIFY(binding.isEmpty());
    QVERIFY(!result.warningMessage.isEmpty());
}

void QodeAssistTest::testAgentBindingRoundTripsThroughTheChatFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("agent_chat.json");

    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks = {Session::TextBlock{"hello agent"}};

    Session::ConversationHistory history;
    history.append(user);

    const Acp::AgentBinding binding{"claude-code-acp", "sess-42"};
    QVERIFY(Chat::ChatSerializer::saveToFile(history, binding, filePath).success);

    Session::ConversationHistory reloaded;
    Acp::AgentBinding reloadedBinding;
    QVERIFY(Chat::ChatSerializer::loadFromFile(reloaded, reloadedBinding, filePath).success);

    QCOMPARE(reloaded, history);
    QCOMPARE(reloadedBinding, binding);
}

void QodeAssistTest::testChatFileWithoutAnAgentLoadsUnbound()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("llm_chat.json");

    Session::Message user;
    user.role = Session::MessageRole::User;
    user.blocks = {Session::TextBlock{"hello"}};

    Session::ConversationHistory history;
    history.append(user);

    QVERIFY(Chat::ChatSerializer::saveToFile(history, Acp::AgentBinding{}, filePath).success);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(!root.contains("agent"));
    file.close();

    Session::ConversationHistory reloaded;
    Acp::AgentBinding reloadedBinding;
    QVERIFY(Chat::ChatSerializer::loadFromFile(reloaded, reloadedBinding, filePath).success);
    QVERIFY(reloadedBinding.isEmpty());
}

void QodeAssistTest::testAcpBackendMapsToolCallLifecycle()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setToolCallUpdates(
            {QJsonObject{
                 {"toolCallId", QStringLiteral("call-1")},
                 {"title", QStringLiteral("Edit main.cpp")},
                 {"kind", QStringLiteral("edit")},
                 {"status", QStringLiteral("pending")}},
             QJsonObject{
                 {"toolCallId", QStringLiteral("call-1")},
                 {"status", QStringLiteral("completed")},
                 {"locations",
                  QJsonArray{QJsonObject{{"path", QStringLiteral("/p/main.cpp")}, {"line", 12}}}},
                 {"content",
                  QJsonArray{
                      QJsonObject{
                          {"type", QStringLiteral("diff")},
                          {"path", QStringLiteral("/p/main.cpp")},
                          {"oldText", QStringLiteral("int a;")},
                          {"newText", QStringLiteral("int b;")}}}}}});
    };

    fixture.send("edit it");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto updates = fixture.eventsOfType<Session::ToolCallUpdated>();
    QCOMPARE(updates.size(), 2);

    QCOMPARE(updates.at(0).toolId, QString("call-1"));
    QCOMPARE(updates.at(0).name, QString("Edit main.cpp"));
    QCOMPARE(updates.at(0).kind, QString("edit"));
    QCOMPARE(updates.at(0).status, QString("pending"));
    QVERIFY(updates.at(0).details.isEmpty());

    QCOMPARE(updates.at(1).status, QString("completed"));
    QCOMPARE(updates.at(1).name, QString("Edit main.cpp"));

    const QJsonArray locations = updates.at(1).details.value("locations").toArray();
    QCOMPARE(locations.size(), 1);
    QCOMPARE(locations.at(0).toObject().value("path").toString(), QString("/p/main.cpp"));
    QCOMPARE(locations.at(0).toObject().value("line").toInt(), 12);

    const QJsonArray diffs = updates.at(1).details.value("diffs").toArray();
    QCOMPARE(diffs.size(), 1);
    QCOMPARE(diffs.at(0).toObject().value("oldText").toString(), QString("int a;"));
    QCOMPARE(diffs.at(0).toObject().value("newText").toString(), QString("int b;"));
}

void QodeAssistTest::testAcpBackendMapsPlanUpdates()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPlanUpdates(
            {QJsonArray{
                 QJsonObject{
                     {"content", QStringLiteral("Read the file")},
                     {"priority", QStringLiteral("high")},
                     {"status", QStringLiteral("in_progress")}},
                 QJsonObject{
                     {"content", QStringLiteral("Apply the fix")},
                     {"priority", QStringLiteral("medium")},
                     {"status", QStringLiteral("pending")}}},
             QJsonArray{
                 QJsonObject{
                     {"content", QStringLiteral("Read the file")},
                     {"priority", QStringLiteral("high")},
                     {"status", QStringLiteral("completed")}},
                 QJsonObject{
                     {"content", QStringLiteral("Apply the fix")},
                     {"priority", QStringLiteral("medium")},
                     {"status", QStringLiteral("in_progress")}}}});
    };

    fixture.send("fix it");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto plans = fixture.eventsOfType<Session::PlanUpdated>();
    QCOMPARE(plans.size(), 2);
    QCOMPARE(plans.at(0).entries.size(), 2);
    QCOMPARE(
        plans.at(0).entries.at(0),
        (Session::PlanEntry{"Read the file", "high", "in_progress"}));
    QCOMPARE(
        plans.at(1).entries.at(0), (Session::PlanEntry{"Read the file", "high", "completed"}));
}

void QodeAssistTest::testAcpBackendReportsUsageFromThePromptResult()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPromptUsage(
            QJsonObject{
                {"cachedReadTokens", 23119},
                {"cachedWriteTokens", 12271},
                {"inputTokens", 2},
                {"outputTokens", 13},
                {"totalTokens", 35405}});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto usage = fixture.eventsOfType<Session::UsageReported>();
    QCOMPARE(usage.size(), 1);
    QCOMPARE(usage.at(0).usage.promptTokens, 2);
    QCOMPARE(usage.at(0).usage.completionTokens, 13);
    QCOMPARE(usage.at(0).usage.cachedPromptTokens, 23119);
    QCOMPARE(usage.at(0).usage.reasoningTokens, 0);
}

void QodeAssistTest::testAcpBackendIgnoresTheCumulativeContextGauge()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setContextGauge(
            QJsonObject{
                {"size", 967000},
                {"used", 35395},
                {"cost", QJsonObject{{"amount", 0.0807}, {"currency", QStringLiteral("USD")}}}});
    };

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QCOMPARE(fixture.eventsOfType<Session::UsageReported>().size(), 0);
}

void QodeAssistTest::testAcpBackendForwardsTheAgentSuggestedTitle()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) { agent->setSuggestedTitle("Refactor the parser"); };

    QStringList titles;
    QObject::connect(
        &fixture.backend,
        &Acp::AcpChatBackend::agentTitleSuggested,
        &fixture.backend,
        [&titles](const QString &title) { titles.append(title); });

    fixture.send("hi");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);
    QTRY_COMPARE(titles.size(), 1);
    QCOMPARE(titles.at(0), QString("Refactor the parser"));
}

void QodeAssistTest::testAcpBackendDeclinesAmbiguousPermissionOptions()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(
            writeToolCall(),
            QJsonArray{
                QJsonObject{
                    {"optionId", QStringLiteral("x")},
                    {"name", QStringLiteral("Yes, and don't ask again")},
                    {"kind", QStringLiteral("allow_always")}},
                QJsonObject{
                    {"optionId", QStringLiteral("x")},
                    {"name", QStringLiteral("No")},
                    {"kind", QStringLiteral("reject_once")}}});
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionResolved>().size(), 1);

    const auto requests = fixture.eventsOfType<Session::PermissionRequested>();
    QCOMPARE(requests.size(), 1);
    QVERIFY(requests.at(0).options.isEmpty());

    QVERIFY(fixture.eventsOfType<Session::PermissionResolved>().at(0).cancelled);
    QTRY_COMPARE(fixture.agent->permissionOutcomes().size(), 1);
    QCOMPARE(
        fixture.agent->permissionOutcomes().at(0).value("outcome").toString(), QString("cancelled"));
}

void QodeAssistTest::testAcpBackendClampsOversizedPermissionText()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        QJsonObject toolCall = writeToolCall();
        toolCall["title"] = QString(5000, QChar('A'));
        agent->setPermissionRequest(toolCall, writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    const Session::PermissionRequested request
        = fixture.eventsOfType<Session::PermissionRequested>().at(0);
    QVERIFY(request.title.size() < 5000);
    QVERIFY(request.title.endsWith(QChar(0x2026)));
    QCOMPARE(request.options.size(), 2);
}

void QodeAssistTest::testAcpBackendCancelsPermissionsWhenTheTurnCompletes()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
        agent->setFinishPromptWithoutWaiting(true);
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::TurnCompleted>().size(), 1);

    const auto resolutions = fixture.eventsOfType<Session::PermissionResolved>();
    QCOMPARE(resolutions.size(), 1);
    QVERIFY(resolutions.at(0).cancelled);

    const QString requestId = fixture.eventsOfType<Session::PermissionRequested>().at(0).requestId;
    QVERIFY(!fixture.backend.respondPermission(requestId, "allow"));
}

void QodeAssistTest::testAcpBackendCancelsPermissionRequestsWhenTheTurnFails()
{
    AcpBackendFixture fixture;
    fixture.configure = [](FakeAcpAgent *agent) {
        agent->setPermissionRequest(writeToolCall(), writePermissionOptions());
    };

    fixture.send("edit main.cpp");

    QTRY_COMPARE(fixture.eventsOfType<Session::PermissionRequested>().size(), 1);

    fixture.backend.clearToolSession(QString());

    QCOMPARE(fixture.eventsOfType<Session::PermissionResolved>().size(), 1);
    QVERIFY(fixture.eventsOfType<Session::PermissionResolved>().at(0).cancelled);
}

} // namespace QodeAssist
