// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "QuickRefactorHandler.hpp"

#include <memory>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ContentBlocks.hpp>
#include <LLMQore/ToolLoopRunner.hpp>
#include <LLMQore/ToolsManager.hpp>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <utils/filepath.h>

#include <context/DocumentContextReader.hpp>
#include <context/DocumentReaderQtCreator.hpp>
#include <context/EnvBlockFormatter.hpp>
#include <context/Utils.hpp>
#include <logger/Logger.hpp>
#include <sources/common/ResponseCleaner.hpp>
#include <settings/GeneralSettings.hpp>
#include <settings/QuickRefactorSettings.hpp>
#include <settings/ToolsSettings.hpp>

#include "sources/common/ContextData.hpp"

#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <ContextRenderer.hpp>
#include <ConversationHistory.hpp>
#include <Session.hpp>
#include <SessionManager.hpp>
#include <SystemPromptBuilder.hpp>

#include "sources/settings/PipelinesConfig.hpp"
#include "tools/ToolsRegistration.hpp"

namespace QodeAssist {

QuickRefactorHandler::QuickRefactorHandler(QObject *parent)
    : QObject(parent)
    , m_currentEditor(nullptr)
    , m_isRefactoringInProgress(false)
    , m_contextManager(this)
{
}

QuickRefactorHandler::~QuickRefactorHandler() {}

void QuickRefactorHandler::setSessionManager(SessionManager *sessionManager)
{
    m_sessionManager = sessionManager;
}

void QuickRefactorHandler::setAgentFactory(AgentFactory *agentFactory)
{
    m_agentFactory = agentFactory;
}

void QuickRefactorHandler::sendRefactorRequest(
    TextEditor::TextEditorWidget *editor, const QString &instructions)
{
    if (m_isRefactoringInProgress) {
        cancelRequest();
    }

    m_currentEditor = editor;

    Utils::Text::Range range;
    if (editor->textCursor().hasSelection()) {
        QTextCursor cursor = editor->textCursor();
        int startPos = cursor.selectionStart();
        int endPos = cursor.selectionEnd();

        QTextBlock startBlock = editor->document()->findBlock(startPos);
        int startLine = startBlock.blockNumber() + 1;
        int startColumn = startPos - startBlock.position();

        QTextBlock endBlock = editor->document()->findBlock(endPos);
        int endLine = endBlock.blockNumber() + 1;
        int endColumn = endPos - endBlock.position();

        Utils::Text::Position startPosition;
        startPosition.line = startLine;
        startPosition.column = startColumn;

        Utils::Text::Position endPosition;
        endPosition.line = endLine;
        endPosition.column = endColumn;

        range = Utils::Text::Range();
        range.begin = startPosition;
        range.end = endPosition;
    } else {
        QTextCursor cursor = editor->textCursor();
        int cursorPos = cursor.position();

        QTextBlock block = editor->document()->findBlock(cursorPos);
        int line = block.blockNumber() + 1;
        int column = cursorPos - block.position();

        Utils::Text::Position cursorPosition;
        cursorPosition.line = line;
        cursorPosition.column = column;
        range = Utils::Text::Range();
        range.begin = cursorPosition;
        range.end = cursorPosition;
    }

    m_currentRange = range;
    prepareAndSendRequest(editor, instructions, range);
}

QString QuickRefactorHandler::configuredAgent(AgentFactory *agentFactory)
{
    const QString configured = Settings::PipelinesConfig::load().rosters.quickRefactor;
    if (configured.isEmpty() || !agentFactory || !agentFactory->configByName(configured))
        return {};
    return configured;
}

QString QuickRefactorHandler::pickRefactorAgent() const
{
    return configuredAgent(m_agentFactory);
}

void QuickRefactorHandler::prepareAndSendRequest(
    TextEditor::TextEditorWidget *editor,
    const QString &instructions,
    const Utils::Text::Range &range)
{
    const auto emitError = [this, editor](const QString &error) {
        LOG_MESSAGE(error);
        RefactorResult result;
        result.success = false;
        result.errorMessage = error;
        result.editor = editor;
        emit refactoringCompleted(result);
    };

    if (!m_sessionManager) {
        emitError(QStringLiteral("Quick refactor session manager is not available"));
        return;
    }

    const QString agentName = pickRefactorAgent();
    if (agentName.isEmpty()) {
        emitError(QStringLiteral(
            "No quick refactor agent configured. Set one in QodeAssist > General."));
        return;
    }

    QString sessionError;
    Session *session = m_sessionManager->acquire(agentName, &sessionError);
    if (!session) {
        emitError(sessionError.isEmpty() ? QStringLiteral("No quick refactor agent selected")
                                         : sessionError);
        return;
    }

    auto *client = session->client();
    if (!client) {
        m_sessionManager->removeSession(session);
        emitError(QStringLiteral("Quick refactor agent has no live client"));
        return;
    }

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    Templates::ContextRenderer::Bindings bindings;
    bindings.projectDir = project ? project->projectDirectory().toFSPathString() : QString();
    bindings.configDir = AgentFactory::userConfigDir();
    session->setContextBindings(bindings);

    const AgentConfig *agentConfig
        = m_agentFactory ? m_agentFactory->configByName(agentName) : nullptr;
    if (agentConfig && agentConfig->enableTools) {
        m_sessionManager->toolContributors().contribute(client->tools());
        client->toolLoop()->setMaxRounds(Settings::toolsSettings().maxToolContinuations());
    }

    session->systemPrompt()->setLayer(
        QStringLiteral("refactor"), buildContextLayer(editor, range));

    client->setTransferTimeout(
        static_cast<int>(Settings::generalSettings().requestTimeout() * 1000));

    m_isRefactoringInProgress = true;

    connect(
        session, &Session::finished, this,
        [this](const LLMQore::RequestID &id, const QString &) { onRefactorFinished(id); });
    connect(
        session, &Session::failed, this,
        [this](const LLMQore::RequestID &id, const QodeAssist::ErrorInfo &error) {
            onRefactorFailed(id, error);
        });

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    const QString userMessage = instructions.isEmpty()
        ? QStringLiteral("Refactor the code to improve its quality and maintainability.")
        : instructions;
    blocks.push_back(std::make_unique<LLMQore::TextContent>(userMessage));

    const LLMQore::RequestID requestId = session->send(std::move(blocks));
    if (requestId.isEmpty()) {
        m_isRefactoringInProgress = false;
        const QString reason = session->lastError().message;
        m_sessionManager->removeSession(session);
        emitError(QStringLiteral("Failed to start quick refactor request for agent '%1': %2")
                      .arg(agentName, reason));
        return;
    }

    m_lastRequestId = requestId;
    m_activeRequests[requestId] = {QJsonObject{{"id", requestId}}, session};
}

QString QuickRefactorHandler::buildContextLayer(
    TextEditor::TextEditorWidget *editor, const Utils::Text::Range &range)
{
    Q_UNUSED(range)

    auto textDocument = editor->textDocument();
    Context::DocumentReaderQtCreator documentReader;
    auto documentInfo = documentReader.readDocument(textDocument->filePath().toUrlishString());

    if (!documentInfo.document) {
        LOG_MESSAGE("Error: Document is not available");
        return {};
    }

    QTextCursor cursor = editor->textCursor();
    int cursorPos = cursor.position();

    Context::DocumentContextReader
        reader(documentInfo.document, documentInfo.mimeType, documentInfo.filePath);

    QString taggedContent;
    bool readFullFile = Settings::quickRefactorSettings().readFullFile();

    if (cursor.hasSelection()) {
        int selStart = cursor.selectionStart();
        int selEnd = cursor.selectionEnd();

        QTextBlock startBlock = documentInfo.document->findBlock(selStart);
        int startLine = startBlock.blockNumber();
        int startColumn = selStart - startBlock.position();

        QTextBlock endBlock = documentInfo.document->findBlock(selEnd);
        int endLine = endBlock.blockNumber();
        int endColumn = selEnd - endBlock.position();

        QString contextBefore;
        if (readFullFile) {
            contextBefore = reader.readWholeFileBefore(startLine, startColumn);
        } else {
            contextBefore = reader.getContextBefore(
                startLine, startColumn, Settings::quickRefactorSettings().readStringsBeforeCursor() + 1);
        }

        QString selectedText = cursor.selectedText();
        selectedText.replace(QChar(0x2029), "\n");

        QString contextAfter;
        if (readFullFile) {
            contextAfter = reader.readWholeFileAfter(endLine, endColumn);
        } else {
            contextAfter = reader.getContextAfter(
                endLine, endColumn, Settings::quickRefactorSettings().readStringsAfterCursor() + 1);
        }

        taggedContent = contextBefore;
        if (selStart == cursorPos) {
            taggedContent += "<cursor><selection_start>" + selectedText + "<selection_end>";
        } else {
            taggedContent += "<selection_start>" + selectedText + "<selection_end><cursor>";
        }
        taggedContent += contextAfter;
    } else {
        QTextBlock block = documentInfo.document->findBlock(cursorPos);
        int line = block.blockNumber();
        int column = cursorPos - block.position();

        QString contextBefore;
        if (readFullFile) {
            contextBefore = reader.readWholeFileBefore(line, column);
        } else {
            contextBefore = reader.getContextBefore(
                line, column, Settings::quickRefactorSettings().readStringsBeforeCursor() + 1);
        }

        QString contextAfter;
        if (readFullFile) {
            contextAfter = reader.readWholeFileAfter(line, column);
        } else {
            contextAfter = reader.getContextAfter(
                line, column, Settings::quickRefactorSettings().readStringsAfterCursor() + 1);
        }

        taggedContent = contextBefore + "<cursor>" + contextAfter;
    }

    QString contextLayer = Context::EnvBlockFormatter::formatFile(
        {documentInfo.filePath, documentInfo.mimeType});

    contextLayer += "\n# Code Context with Position Markers\n" + taggedContent;

    contextLayer += "\n\n# What to Generate:";
    contextLayer += cursor.hasSelection()
        ? "\n- Generate ONLY the code that should REPLACE the selected text between "
          "<selection_start> and <selection_end> markers"
          "\n- Your output will completely replace the selected code"
        : "\n- Generate ONLY the code that should be INSERTED at the <cursor> position"
          "\n- Your output will be inserted at the cursor location";

    QString indentNote;
    if (cursor.hasSelection()) {
        QTextBlock startBlock = documentInfo.document->findBlock(cursor.selectionStart());
        int leadingSpaces = 0;
        for (QChar c : startBlock.text()) {
            if (c == ' ') leadingSpaces++;
            else if (c == '\t') leadingSpaces += 4;
            else break;
        }
        if (leadingSpaces > 0) {
            indentNote = QString("\n- CRITICAL: The code to replace starts with %1 spaces of indentation"
                                 "\n- Your output MUST start with exactly %1 spaces (or equivalent tabs)"
                                 "\n- Each line in your output must maintain this base indentation")
                             .arg(leadingSpaces);
        }
        indentNote += "\n- PRESERVE all indentation from the original code";
    } else {
        QTextBlock block = documentInfo.document->findBlock(cursorPos);
        QString lineText = block.text();
        int leadingSpaces = 0;
        for (QChar c : lineText) {
            if (c == ' ') leadingSpaces++;
            else if (c == '\t') leadingSpaces += 4;
            else break;
        }
        if (leadingSpaces > 0) {
            indentNote = QString("\n- CRITICAL: Current line has %1 spaces of indentation"
                                 "\n- If generating multiline code, EVERY line must start with at least %1 spaces"
                                 "\n- If generating single-line code, it will be inserted inline (no indentation needed)")
                             .arg(leadingSpaces);
        }
    }
    if (!indentNote.isEmpty())
        contextLayer += "\n\n## Indentation:" + indentNote;

    if (Settings::quickRefactorSettings().useOpenFilesInQuickRefactor()) {
        contextLayer += "\n\n" + m_contextManager.openedFilesContext({documentInfo.filePath});
    }

    return contextLayer;
}

void QuickRefactorHandler::cancelRequest()
{
    if (!m_isRefactoringInProgress)
        return;

    const auto id = m_lastRequestId;
    m_isRefactoringInProgress = false;
    m_lastRequestId.clear();

    auto it = m_activeRequests.find(id);
    if (it != m_activeRequests.end()) {
        Session *session = it.value().session;
        m_activeRequests.erase(it);
        if (session && m_sessionManager)
            m_sessionManager->release(session);
    }

    RefactorResult result;
    result.success = false;
    result.errorMessage = "Refactoring request was cancelled";
    emit refactoringCompleted(result);
}

void QuickRefactorHandler::onRefactorFinished(const QString &requestId)
{
    if (requestId != m_lastRequestId)
        return;

    auto it = m_activeRequests.find(requestId);
    Session *session = (it != m_activeRequests.end()) ? it.value().session.data() : nullptr;
    if (it != m_activeRequests.end())
        m_activeRequests.erase(it);

    QString fullText;
    if (session) {
        if (auto *history = session->history(); history && !history->isEmpty())
            fullText = history->messages().back().text();
    }

    m_isRefactoringInProgress = false;
    m_lastRequestId.clear();

    const QString cleanedResponse = ResponseCleaner::clean(fullText);

    RefactorResult result;
    result.newText = cleanedResponse;
    result.insertRange = m_currentRange;
    result.success = true;
    result.editor = m_currentEditor;

    LOG_MESSAGE("Refactoring completed successfully. New code to insert: ");
    LOG_MESSAGE("---------- BEGIN REFACTORED CODE ----------");
    LOG_MESSAGE(cleanedResponse);
    LOG_MESSAGE("----------- END REFACTORED CODE -----------");

    emit refactoringCompleted(result);

    if (session && m_sessionManager)
        m_sessionManager->release(session);
}

void QuickRefactorHandler::onRefactorFailed(
    const QString &requestId, const QodeAssist::ErrorInfo &error)
{
    if (requestId != m_lastRequestId)
        return;

    auto it = m_activeRequests.find(requestId);
    Session *session = (it != m_activeRequests.end()) ? it.value().session.data() : nullptr;
    if (it != m_activeRequests.end())
        m_activeRequests.erase(it);

    m_isRefactoringInProgress = false;
    m_lastRequestId.clear();

    RefactorResult result;
    result.success = false;
    result.errorMessage = error.message;
    result.editor = m_currentEditor;
    emit refactoringCompleted(result);

    if (session && m_sessionManager)
        m_sessionManager->release(session);
}

} // namespace QodeAssist
