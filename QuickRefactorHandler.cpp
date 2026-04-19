/*
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "QuickRefactorHandler.hpp"

#include <LLMQore/BaseClient.hpp>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <context/DocumentContextReader.hpp>
#include <pluginllmcore/ResponseCleaner.hpp>
#include <context/DocumentReaderQtCreator.hpp>
#include <context/Utils.hpp>
#include <pluginllmcore/PromptTemplateManager.hpp>
#include <pluginllmcore/ProvidersManager.hpp>
#include <pluginllmcore/RulesLoader.hpp>
#include <logger/Logger.hpp>
#include <settings/ChatAssistantSettings.hpp>
#include <settings/GeneralSettings.hpp>
#include <settings/QuickRefactorSettings.hpp>

namespace QodeAssist {

QuickRefactorHandler::QuickRefactorHandler(QObject *parent)
    : QObject(parent)
    , m_currentEditor(nullptr)
    , m_isRefactoringInProgress(false)
    , m_contextManager(this)
{
}

QuickRefactorHandler::~QuickRefactorHandler() {}

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

void QuickRefactorHandler::prepareAndSendRequest(
    TextEditor::TextEditorWidget *editor,
    const QString &instructions,
    const Utils::Text::Range &range)
{
    auto &settings = Settings::generalSettings();

    auto &providerRegistry = PluginLLMCore::ProvidersManager::instance();
    auto &promptManager = PluginLLMCore::PromptTemplateManager::instance();

    const auto providerName = settings.qrProvider();
    auto provider = providerRegistry.getProviderByName(providerName);

    if (!provider) {
        QString error = QString("No provider found with name: %1").arg(providerName);
        LOG_MESSAGE(error);
        RefactorResult result;
        result.success = false;
        result.errorMessage = error;
        result.editor = editor;
        emit refactoringCompleted(result);
        return;
    }

    const auto templateName = settings.qrTemplate();
    auto promptTemplate = promptManager.getChatTemplateByName(templateName);

    if (!promptTemplate) {
        QString error = QString("No template found with name: %1").arg(templateName);
        LOG_MESSAGE(error);
        RefactorResult result;
        result.success = false;
        result.errorMessage = error;
        result.editor = editor;
        emit refactoringCompleted(result);
        return;
    }

    QJsonObject payload{
        {"model", Settings::generalSettings().qrModel()}, {"stream", true}};

    PluginLLMCore::ContextData context = prepareContext(editor, range, instructions);

    bool enableTools = Settings::quickRefactorSettings().useTools();
    bool enableThinking = Settings::quickRefactorSettings().useThinking();
    provider->prepareRequest(
        payload,
        promptTemplate,
        context,
        PluginLLMCore::RequestType::QuickRefactoring,
        enableTools,
        enableThinking);

    m_isRefactoringInProgress = true;

    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestCompleted,
        this,
        &QuickRefactorHandler::handleFullResponse,
        Qt::UniqueConnection);

    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestFailed,
        this,
        &QuickRefactorHandler::handleRequestFailed,
        Qt::UniqueConnection);

    auto requestId = provider->sendRequest(
        QUrl(Settings::generalSettings().qrUrl()), payload, promptTemplate->endpoint());
    m_lastRequestId = requestId;
    QJsonObject request{{"id", requestId}};

    m_activeRequests[requestId] = {request, provider};
}

PluginLLMCore::ContextData QuickRefactorHandler::prepareContext(
    TextEditor::TextEditorWidget *editor,
    const Utils::Text::Range &range,
    const QString &instructions)
{
    PluginLLMCore::ContextData context;

    auto textDocument = editor->textDocument();
    Context::DocumentReaderQtCreator documentReader;
    auto documentInfo = documentReader.readDocument(textDocument->filePath().toUrlishString());

    if (!documentInfo.document) {
        LOG_MESSAGE("Error: Document is not available");
        return context;
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

    QString systemPrompt = Settings::quickRefactorSettings().systemPrompt();

    auto project = PluginLLMCore::RulesLoader::getActiveProject();
    if (project) {
        QString projectRules = PluginLLMCore::RulesLoader::loadRulesForProject(
            project, PluginLLMCore::RulesContext::QuickRefactor);

        if (!projectRules.isEmpty()) {
            systemPrompt += "\n\n# Project Rules\n\n" + projectRules;
            LOG_MESSAGE("Loaded project rules for quick refactor");
        }
    }

    systemPrompt += "\n\nFile information:";
    systemPrompt += "\nLanguage: " + documentInfo.mimeType;
    systemPrompt += "\nFile path: " + documentInfo.filePath;

    systemPrompt += "\n\n# Code Context with Position Markers\n" + taggedContent;

    systemPrompt += "\n\n# Output Requirements\n## What to Generate:";
    systemPrompt += cursor.hasSelection()
        ? "\n- Generate ONLY the code that should REPLACE the selected text between "
          "<selection_start> and <selection_end> markers"
          "\n- Your output will completely replace the selected code"
        : "\n- Generate ONLY the code that should be INSERTED at the <cursor> position"
          "\n- Your output will be inserted at the cursor location";
    
    systemPrompt += "\n\n## Formatting Rules:"
                    "\n- Output ONLY the code itself, without ANY explanations or descriptions"
                    "\n- Do NOT include markdown code blocks (no ```, no language tags)"
                    "\n- Do NOT add comments explaining what you changed"
                    "\n- Do NOT repeat existing code, be precise with context"
                    "\n- Do NOT send in answer <cursor> or </cursor> and other tags"
                    "\n- The output must be ready to insert directly into the editor as-is";
    
    systemPrompt += "\n\n## Indentation and Whitespace:";
    
    if (cursor.hasSelection()) {
        QTextBlock startBlock = documentInfo.document->findBlock(cursor.selectionStart());
        int leadingSpaces = 0;
        for (QChar c : startBlock.text()) {
            if (c == ' ') leadingSpaces++;
            else if (c == '\t') leadingSpaces += 4;
            else break;
        }
        if (leadingSpaces > 0) {
            systemPrompt += QString("\n- CRITICAL: The code to replace starts with %1 spaces of indentation"
                                   "\n- Your output MUST start with exactly %1 spaces (or equivalent tabs)"
                                   "\n- Each line in your output must maintain this base indentation")
                               .arg(leadingSpaces);
        }
        systemPrompt += "\n- PRESERVE all indentation from the original code";
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
            systemPrompt += QString("\n- CRITICAL: Current line has %1 spaces of indentation"
                                   "\n- If generating multiline code, EVERY line must start with at least %1 spaces"
                                   "\n- If generating single-line code, it will be inserted inline (no indentation needed)")
                               .arg(leadingSpaces);
        }
    }
    
    systemPrompt += "\n- Use the same indentation style (spaces or tabs) as the surrounding code"
                    "\n- Maintain consistent indentation for nested blocks"
                    "\n- Do NOT remove or reduce the base indentation level"
                    "\n\n## Code Style:"
                    "\n- Match the coding style of the surrounding code (naming, spacing, braces, etc.)"
                    "\n- Preserve the original code structure when possible"
                    "\n- Only change what is necessary to fulfill the user's request";

    if (Settings::quickRefactorSettings().useOpenFilesInQuickRefactor()) {
        systemPrompt += "\n\n" + m_contextManager.openedFilesContext({documentInfo.filePath});
    }

    context.systemPrompt = systemPrompt;

    QVector<PluginLLMCore::Message> messages;
    messages.append(
        {"user",
         instructions.isEmpty() ? "Refactor the code to improve its quality and maintainability."
                                : instructions});
    context.history = messages;

    return context;
}

void QuickRefactorHandler::handleLLMResponse(
    const QString &response, const QJsonObject &request, bool isComplete)
{
    if (request["id"].toString() != m_lastRequestId) {
        return;
    }

    if (isComplete) {
        m_isRefactoringInProgress = false;
        QString cleanedResponse = PluginLLMCore::ResponseCleaner::clean(response);

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
    }
}

void QuickRefactorHandler::cancelRequest()
{
    if (m_isRefactoringInProgress) {
        auto id = m_lastRequestId;

        for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
            if (it.key() == id) {
                const RequestContext &ctx = it.value();
                ctx.provider->cancelRequest(id);
                m_activeRequests.erase(it);
                break;
            }
        }

        m_isRefactoringInProgress = false;

        RefactorResult result;
        result.success = false;
        result.errorMessage = "Refactoring request was cancelled";
        emit refactoringCompleted(result);
    }
}

void QuickRefactorHandler::handleFullResponse(const QString &requestId, const QString &fullText)
{
    if (requestId == m_lastRequestId) {
        m_activeRequests.remove(requestId);
        QJsonObject request{{"id", requestId}};
        handleLLMResponse(fullText, request, true);
    }
}

void QuickRefactorHandler::handleRequestFailed(const QString &requestId, const QString &error)
{
    if (requestId == m_lastRequestId) {
        auto it = m_activeRequests.find(requestId);
        QString enriched = (it != m_activeRequests.end() && it->provider)
                               ? it->provider->enrichErrorMessage(error)
                               : error;

        m_activeRequests.remove(requestId);
        m_isRefactoringInProgress = false;
        RefactorResult result;
        result.success = false;
        result.errorMessage = enriched;
        result.editor = m_currentEditor;
        emit refactoringCompleted(result);
    }
}

} // namespace QodeAssist
