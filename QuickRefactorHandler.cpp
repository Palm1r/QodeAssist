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

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <context/DocumentContextReader.hpp>
#include <context/DocumentReaderQtCreator.hpp>
#include <context/Utils.hpp>
#include <llmcore/PromptTemplateManager.hpp>
#include <llmcore/ProvidersManager.hpp>
#include <llmcore/RequestConfig.hpp>
#include <logger/Logger.hpp>
#include <settings/ChatAssistantSettings.hpp>
#include <settings/GeneralSettings.hpp>

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

    auto &providerRegistry = LLMCore::ProvidersManager::instance();
    auto &promptManager = LLMCore::PromptTemplateManager::instance();

    const auto providerName = settings.caProvider();
    auto provider = providerRegistry.getProviderByName(providerName);

    if (!provider) {
        LOG_MESSAGE(QString("No provider found with name: %1").arg(providerName));
        RefactorResult result;
        result.success = false;
        result.errorMessage = QString("No provider found with name: %1").arg(providerName);
        emit refactoringCompleted(result);
        return;
    }

    const auto templateName = settings.caTemplate();
    auto promptTemplate = promptManager.getChatTemplateByName(templateName);

    if (!promptTemplate) {
        LOG_MESSAGE(QString("No template found with name: %1").arg(templateName));
        RefactorResult result;
        result.success = false;
        result.errorMessage = QString("No template found with name: %1").arg(templateName);
        emit refactoringCompleted(result);
        return;
    }

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Chat;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    config.url = QString("%1%2").arg(settings.caUrl(), provider->chatEndpoint());
    config.providerRequest = {{"model", settings.caModel()}, {"stream", true}};
    config.apiKey = provider->apiKey();

    LLMCore::ContextData context = prepareContext(editor, range, instructions);

    provider
        ->prepareRequest(config.providerRequest, promptTemplate, context, LLMCore::RequestType::Chat);

    QString requestId = QUuid::createUuid().toString();
    m_lastRequestId = requestId;
    QJsonObject request{{"id", requestId}};

    m_isRefactoringInProgress = true;

    m_activeRequests[requestId] = {request, provider};

    connect(
        provider,
        &LLMCore::Provider::fullResponseReceived,
        this,
        &QuickRefactorHandler::handleFullResponse,
        Qt::UniqueConnection);

    connect(
        provider,
        &LLMCore::Provider::requestFailed,
        this,
        &QuickRefactorHandler::handleRequestFailed,
        Qt::UniqueConnection);

    provider->sendRequest(requestId, config.url, config.providerRequest);
}

LLMCore::ContextData QuickRefactorHandler::prepareContext(
    TextEditor::TextEditorWidget *editor,
    const Utils::Text::Range &range,
    const QString &instructions)
{
    LLMCore::ContextData context;

    auto textDocument = editor->textDocument();
    Context::DocumentReaderQtCreator documentReader;
    auto documentInfo = documentReader.readDocument(textDocument->filePath().toUrlishString());

    if (!documentInfo.document) {
        LOG_MESSAGE("Error: Document is not available");
        return context;
    }

    QTextCursor cursor = editor->textCursor();
    int cursorPos = cursor.position();

    // TODO add selecting content before and after cursor/selection
    QString fullContent = documentInfo.document->toPlainText();
    QString taggedContent = fullContent;

    if (cursor.hasSelection()) {
        int selEnd = cursor.selectionEnd();
        int selStart = cursor.selectionStart();
        taggedContent
            .insert(selEnd, selEnd == cursorPos ? "<selection_end><cursor>" : "<selection_end>");
        taggedContent.insert(
            selStart, selStart == cursorPos ? "<cursor><selection_start>" : "<selection_start>");
    } else {
        taggedContent.insert(cursorPos, "<cursor>");
    }

    QString systemPrompt = Settings::codeCompletionSettings().quickRefactorSystemPrompt();
    systemPrompt += "\n\nFile information:";
    systemPrompt += "\nLanguage: " + documentInfo.mimeType;
    systemPrompt += "\nFile path: " + documentInfo.filePath;

    systemPrompt += "\n\nCode context with position markers:";
    systemPrompt += taggedContent;

    systemPrompt += "\n\nOutput format:";
    systemPrompt += "\n- Generate ONLY the code that should replace the current selection "
                    "between<selection_start><selection_end> or be "
                    "inserted at cursor position<cursor>";
    systemPrompt += "\n- Do not include any explanations, comments about the code, or markdown "
                    "code block markers";
    systemPrompt += "\n- The output should be ready to insert directly into the editor";
    systemPrompt += "\n- Follow the existing code style and indentation patterns";

    if (Settings::codeCompletionSettings().useOpenFilesInQuickRefactor()) {
        systemPrompt += "\n\n" + m_contextManager.openedFilesContext({documentInfo.filePath});
    }

    context.systemPrompt = systemPrompt;

    QVector<LLMCore::Message> messages;
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
        QString cleanedResponse = response.trimmed();
        if (cleanedResponse.startsWith("```")) {
            int firstNewLine = cleanedResponse.indexOf('\n');
            int lastFence = cleanedResponse.lastIndexOf("```");

            if (firstNewLine != -1 && lastFence > firstNewLine) {
                cleanedResponse
                    = cleanedResponse.mid(firstNewLine + 1, lastFence - firstNewLine - 1).trimmed();
            } else if (lastFence != -1) {
                cleanedResponse = cleanedResponse.mid(3, lastFence - 3).trimmed();
            }
        }

        RefactorResult result;
        result.newText = cleanedResponse;
        result.insertRange = m_currentRange;
        result.success = true;

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
        QJsonObject request{{"id", requestId}};
        handleLLMResponse(fullText, request, true);
    }
}

void QuickRefactorHandler::handleRequestFailed(const QString &requestId, const QString &error)
{
    if (requestId == m_lastRequestId) {
        m_isRefactoringInProgress = false;
        RefactorResult result;
        result.success = false;
        result.errorMessage = error;
        emit refactoringCompleted(result);
    }
}

} // namespace QodeAssist
