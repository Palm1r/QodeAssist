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
#include <logger/Logger.hpp>
#include <settings/ChatAssistantSettings.hpp>
#include <settings/GeneralSettings.hpp>

namespace QodeAssist {

QuickRefactorHandler::QuickRefactorHandler(QObject *parent)
    : QObject(parent)
    , m_requestHandler(new LLMCore::RequestHandler(this))
    , m_currentEditor(nullptr)
    , m_isRefactoringInProgress(false)
    , m_contextManager(this)
{
    connect(
        m_requestHandler,
        &LLMCore::RequestHandler::completionReceived,
        this,
        &QuickRefactorHandler::handleLLMResponse);

    connect(
        m_requestHandler,
        &LLMCore::RequestHandler::requestFinished,
        this,
        [this](const QString &requestId, bool success, const QString &errorString) {
            if (!success && requestId == m_lastRequestId) {
                m_isRefactoringInProgress = false;
                RefactorResult result;
                result.success = false;
                result.errorMessage = errorString;
                emit refactoringCompleted(result);
            }
        });
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

        range = Utils::Text::Range(
            Utils::Text::Position(startLine, startColumn),
            Utils::Text::Position(endLine, endColumn));
    } else {
        QTextCursor cursor = editor->textCursor();
        int cursorPos = cursor.position();

        QTextBlock block = editor->document()->findBlock(cursorPos);
        int line = block.blockNumber() + 1;
        int column = cursorPos - block.position();

        Utils::Text::Position cursorPosition(line, column);
        range = Utils::Text::Range(cursorPosition, cursorPosition);
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
    config.providerRequest
        = {{"model", settings.caModel()}, {"stream", Settings::chatAssistantSettings().stream()}};
    config.apiKey = provider->apiKey();

    LLMCore::ContextData context = prepareContext(editor, range, instructions);

    provider
        ->prepareRequest(config.providerRequest, promptTemplate, context, LLMCore::RequestType::Chat);

    QString requestId = QUuid::createUuid().toString();
    m_lastRequestId = requestId;
    QJsonObject request{{"id", requestId}};

    m_isRefactoringInProgress = true;

    m_requestHandler->sendLLMRequest(config, request);
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

    // TODO add selecting content before and after cursor/selection and from others opened files too
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

    QString systemPrompt = quickRefactorSystemPrompt();
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

    if (Settings::codeCompletionSettings().useOpenFilesContext()) {
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
        m_isRefactoringInProgress = false;

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
    } else {
        emit refactoringProgress(response);
    }
}

void QuickRefactorHandler::cancelRequest()
{
    if (m_isRefactoringInProgress) {
        m_requestHandler->cancelRequest(m_lastRequestId);
        m_isRefactoringInProgress = false;

        RefactorResult result;
        result.success = false;
        result.errorMessage = "Refactoring request was cancelled";
        emit refactoringCompleted(result);
    }
}

bool QuickRefactorHandler::isRefactoringInProgress() const
{
    return m_isRefactoringInProgress;
}

QString QuickRefactorHandler::quickRefactorSystemPrompt() const
{
    return QString("You are an expert code refactoring assistant specializing in C++, Qt, and QML "
                   "development.");
}

} // namespace QodeAssist
