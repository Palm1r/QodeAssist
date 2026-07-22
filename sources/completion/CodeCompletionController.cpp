/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024-2026 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * The Qt Company portions:
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
 *
 * Petr Mironychev portions:
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
 *
 * Additional attribution terms under GPLv3 §7(b) apply — see LICENSE
 */

#include "completion/CodeCompletionController.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QTimer>

#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <projectexplorer/projectmanager.h>

#include "completion/LLMSuggestion.hpp"
#include "refactor/RefactorContextHelper.hpp"
#include "refactor/RefactorSuggestion.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProjectSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "settings/SettingsConstants.hpp"
#include <logger/Logger.hpp>

using namespace TextEditor;
using namespace Utils;
using namespace ProjectExplorer;
using namespace Core;

namespace QodeAssist {

namespace {
bool isIdentifierChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

bool isInsideIdentifier(const QTextCursor &cursor)
{
    const QTextBlock block = cursor.block();
    const int col = cursor.positionInBlock();
    const QString text = block.text();
    if (col <= 0 || col > text.size())
        return false;
    if (!isIdentifierChar(text.at(col - 1)))
        return false;
    return col < text.size() && isIdentifierChar(text.at(col));
}

bool isAfterMemberAccess(const QTextCursor &cursor)
{
    const QTextBlock block = cursor.block();
    const int col = cursor.positionInBlock();
    const QString text = block.text();
    if (col <= 0)
        return false;

    int i = col - 1;
    while (i >= 0 && isIdentifierChar(text.at(i)))
        --i;

    if (i < 0)
        return false;

    const QChar c = text.at(i);
    if (c == QLatin1Char('.'))
        return true;
    if (c == QLatin1Char('>') && i >= 1 && text.at(i - 1) == QLatin1Char('-'))
        return true;
    if (c == QLatin1Char(':') && i >= 1 && text.at(i - 1) == QLatin1Char(':'))
        return true;
    return false;
}

bool isFreshIndentedLine(const QTextCursor &cursor)
{
    const QTextBlock block = cursor.block();
    const int col = cursor.positionInBlock();
    if (col == 0)
        return false;
    const QString leftText = block.text().left(col);
    for (const QChar &ch : leftText) {
        if (!ch.isSpace())
            return false;
    }
    return true;
}

bool isAfterEagerTrigger(const QTextCursor &cursor)
{
    const QTextBlock block = cursor.block();
    const int col = cursor.positionInBlock();
    const QString text = block.text();
    int i = col - 1;
    while (i >= 0 && text.at(i).isSpace())
        --i;
    if (i < 0)
        return false;
    const QChar c = text.at(i);
    return c == QLatin1Char('{') || c == QLatin1Char('(') || c == QLatin1Char(',')
           || c == QLatin1Char('=') || c == QLatin1Char('[') || c == QLatin1Char(';')
           || c == QLatin1Char(':') || c == QLatin1Char('>');
}

bool isManualTriggerMode()
{
    return Settings::codeCompletionSettings().triggerMode.stringValue() == "Manual";
}

bool isAgenticMode()
{
    const QString mode = Settings::codeCompletionSettings().completionMode.stringValue();
    return mode == QLatin1StringView(Constants::CC_MODE_AGENTIC_LOCAL)
           || mode == QLatin1StringView(Constants::CC_MODE_AGENTIC_ACP);
}
} // anonymous namespace

CodeCompletionController::CodeCompletionController(
    CompletionEngine *fimEngine,
    CompletionEngine *agenticEngine,
    CompletionEngine *acpEngine,
    Context::ContextManager &contextManager,
    QObject *parent)
    : QObject(parent)
    , m_fimEngine(fimEngine)
    , m_agenticEngine(agenticEngine)
    , m_acpEngine(acpEngine)
    , m_contextManager(contextManager)
    , m_recentCharCount(0)
{
    setupConnections();

    m_typingTimer.start();

    m_refactorHoverHandler = new RefactorSuggestionHoverHandler();
    m_refactorWidgetHandler = new RefactorWidgetHandler(this);
}

CodeCompletionController::~CodeCompletionController()
{
    cleanupConnections();
    delete m_refactorHoverHandler;
    delete m_refactorWidgetHandler;
}

void CodeCompletionController::openDocument(TextEditor::TextDocument *document)
{
    if (m_documentConnections.contains(document))
        return;

    auto project = ProjectManager::projectForFile(document->filePath());
    if (!isEnabled(project))
        return;

    auto editors = TextEditor::BaseTextEditor::textEditorsForDocument(document);
    for (auto *editor : editors) {
        if (auto *widget = editor->editorWidget()) {
            widget->addHoverHandler(m_refactorHoverHandler);
            widget->installEventFilter(this);
            if (!m_hoverHandlerWidgets.contains(widget))
                m_hoverHandlerWidgets.append(widget);
        }
    }

    auto connection = connect(
        document,
        &TextDocument::contentsChangedWithPosition,
        this,
        [this, document](int position, int charsRemoved, int charsAdded) {
            if (!Settings::codeCompletionSettings().autoCompletion())
                return;

            if (isManualTriggerMode() || isAgenticMode())
                return;

            auto project = ProjectManager::projectForFile(document->filePath());
            if (!isEnabled(project))
                return;

            auto textEditor = BaseTextEditor::currentTextEditor();
            if (!textEditor || textEditor->document() != document)
                return;

            TextEditorWidget *widget = textEditor->editorWidget();
            if (widget->isReadOnly() || widget->multiTextCursor().hasMultipleCursors())
                return;

            const int cursorPosition = widget->textCursor().position();
            if (cursorPosition < position || cursorPosition > position + charsAdded)
                return;

            if (charsRemoved > 0 || charsAdded <= 0) {
                m_recentCharCount = 0;
                m_typingTimer.restart();
                return;
            }

            QTextCursor cursor = widget->textCursor();
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
            const QString lastChar = cursor.selectedText();
            if (lastChar.isEmpty())
                return;

            const QChar lastCh = lastChar[0];
            if (lastCh == QLatin1Char('\n') || lastCh == QChar::ParagraphSeparator
                || lastCh == QChar::LineSeparator) {
                m_recentCharCount = 0;
                m_typingTimer.restart();
                return;
            }

            const bool isSpaceOrTab = lastCh.isSpace();
            const bool ignoreWhitespace
                = Settings::codeCompletionSettings().ignoreWhitespaceInCharCount();

            if (!ignoreWhitespace || !isSpaceOrTab)
                m_recentCharCount += charsAdded;

            if (m_typingTimer.elapsed()
                > Settings::codeCompletionSettings().autoCompletionTypingInterval()) {
                m_recentCharCount = (ignoreWhitespace && isSpaceOrTab) ? 0 : charsAdded;
                m_typingTimer.restart();
            }

            handleAutoRequestTrigger(widget);
        });

    m_documentConnections.insert(document, connection);
}

void CodeCompletionController::closeDocument(TextEditor::TextDocument *document)
{
    auto it = m_documentConnections.find(document);
    if (it == m_documentConnections.end())
        return;
    disconnect(it.value());
    m_documentConnections.erase(it);
}

void CodeCompletionController::requestCompletions(TextEditor::TextEditorWidget *editor)
{
    auto project = ProjectManager::projectForFile(editor->textDocument()->filePath());

    if (!isEnabled(project))
        return;

    if (m_contextManager.ignoreManager()->shouldIgnore(
            editor->textDocument()->filePath().toUrlishString(), project)) {
        LOG_MESSAGE(QString("Ignoring file due to .qodeassistignore: %1")
                        .arg(editor->textDocument()->filePath().toUrlishString()));
        return;
    }

    auto *engine = activeEngine();
    if (!engine)
        return;

    MultiTextCursor cursor = editor->multiTextCursor();
    if (cursor.hasMultipleCursors() || cursor.hasSelection() || editor->suggestionVisible())
        return;

    cancelRunningRequest(editor);

    const auto &settings = Settings::codeCompletionSettings();
    if (settings.abortAssistOnRequest() && !settings.respectQtcPopup())
        editor->abortAssist();

    if (Settings::codeCompletionSettings().showProgressWidget()) {
        m_progressHandler.setCancelCallback([this, editor = QPointer<TextEditorWidget>(editor)]() {
            if (editor) {
                cancelRunningRequest(editor);
            }
        });
        m_progressHandler.showProgress(editor);
    }

    const int position = cursor.mainCursor().position();
    const quint64 requestId = ++m_lastRequestId;
    m_runningRequests[editor] = {requestId, position, engine};
    m_requestEditors[requestId] = editor;

    const QTextBlock block = editor->document()->findBlock(position);
    CompletionContext context;
    context.filePath = editor->textDocument()->filePath().toUrlishString();
    context.line = block.blockNumber();
    context.column = position - block.position();

    engine->request(requestId, context);
}

void CodeCompletionController::requestQuickRefactor(
    TextEditor::TextEditorWidget *editor, const QString &instructions)
{
    auto project = ProjectManager::projectForFile(editor->textDocument()->filePath());

    if (!isEnabled(project))
        return;

    if (m_contextManager.ignoreManager()->shouldIgnore(
            editor->textDocument()->filePath().toUrlishString(), project)) {
        LOG_MESSAGE(QString("Ignoring file due to .qodeassistignore: %1")
                        .arg(editor->textDocument()->filePath().toUrlishString()));
        return;
    }

    if (!m_refactorHandler) {
        m_refactorHandler = new QuickRefactorHandler(this);
        connect(
            m_refactorHandler,
            &QuickRefactorHandler::refactoringCompleted,
            this,
            &CodeCompletionController::handleRefactoringResult);
    }

    m_progressHandler.setCancelCallback([this, editor = QPointer<TextEditorWidget>(editor)]() {
        if (editor && m_refactorHandler) {
            m_refactorHandler->cancelRequest();
            m_progressHandler.hideProgress();
        }
    });
    m_progressHandler.showProgress(editor);
    m_refactorHandler->sendRefactorRequest(editor, instructions);
}

void CodeCompletionController::scheduleRequest(TextEditor::TextEditorWidget *editor)
{
    if (m_runningRequests.contains(editor)) {
        if (Settings::codeCompletionSettings().cancelOnInput())
            cancelRunningRequest(editor);
        else
            return;
    }

    auto it = m_scheduledRequests.find(editor);
    if (it == m_scheduledRequests.end()) {
        auto timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, editor]() {
            if (!editor || m_runningRequests.contains(editor))
                return;
            if (editor->textCursor().position()
                    != m_scheduledRequests[editor]->property("cursorPosition").toInt())
                return;
            requestCompletions(editor);
        });
        connect(editor, &TextEditorWidget::destroyed, this, [this, editor]() {
            delete m_scheduledRequests.take(editor);
            cancelRunningRequest(editor);
        });
        it = m_scheduledRequests.insert(editor, timer);
    }

    it.value()->setProperty("cursorPosition", editor->textCursor().position());
    it.value()->start(Settings::codeCompletionSettings().startSuggestionTimer());
}

void CodeCompletionController::handleCompletionReady(quint64 requestId, const QString &text)
{
    auto editorIt = m_requestEditors.find(requestId);
    if (editorIt == m_requestEditors.end())
        return;

    const QPointer<TextEditorWidget> editor = editorIt.value();
    m_requestEditors.erase(editorIt);

    if (!editor) {
        dropRunningRequestById(requestId);
        return;
    }

    auto runningIt = m_runningRequests.find(editor);
    if (runningIt == m_runningRequests.end() || runningIt.value().id != requestId)
        return;

    const int requestPosition = runningIt.value().position;
    m_runningRequests.erase(runningIt);

    m_progressHandler.hideProgress();
    const auto &settings = Settings::codeCompletionSettings();
    if (settings.abortAssistOnRequest() && !settings.respectQtcPopup())
        editor->abortAssist();

    applyCompletion(editor, text, requestPosition);
}

void CodeCompletionController::handleCompletionFailed(quint64 requestId, const QString &error)
{
    auto editorIt = m_requestEditors.find(requestId);
    if (editorIt == m_requestEditors.end())
        return;

    const QPointer<TextEditorWidget> editor = editorIt.value();
    m_requestEditors.erase(editorIt);

    LOG_MESSAGE(QString("Code completion failed: %1").arg(error));

    if (!editor) {
        dropRunningRequestById(requestId);
        return;
    }

    auto runningIt = m_runningRequests.find(editor);
    if (runningIt == m_runningRequests.end() || runningIt.value().id != requestId)
        return;
    m_runningRequests.erase(runningIt);

    m_progressHandler.hideProgress();
    m_errorHandler.showError(editor, tr("Code completion failed: %1").arg(error));
}

void CodeCompletionController::dropRunningRequestById(quint64 requestId)
{
    for (auto it = m_runningRequests.begin(); it != m_runningRequests.end(); ++it) {
        if (it.value().id == requestId) {
            m_runningRequests.erase(it);
            return;
        }
    }
}

void CodeCompletionController::applyCompletion(
    TextEditor::TextEditorWidget *editor, const QString &text, int requestPosition)
{
    const MultiTextCursor cursors = editor->multiTextCursor();
    if (cursors.hasMultipleCursors() || cursors.hasSelection())
        return;

    const int currentPosition = cursors.mainCursor().position();
    if (requestPosition < 0 || currentPosition < requestPosition)
        return;

    QString typedSinceRequest;
    if (currentPosition > requestPosition) {
        QTextCursor diffCursor(editor->document());
        diffCursor.setPosition(requestPosition);
        diffCursor.setPosition(currentPosition, QTextCursor::KeepAnchor);
        typedSinceRequest = diffCursor.selectedText();
        if (typedSinceRequest.contains(QChar::ParagraphSeparator)
            || typedSinceRequest.contains(QLatin1Char('\n'))) {
            return;
        }
    }

    QString completionText = text;
    const int end = int(completionText.size()) - 1;
    int delta = 0;
    while (delta <= end && completionText[end - delta].isSpace())
        ++delta;
    if (delta > 0)
        completionText.chop(delta);

    if (!typedSinceRequest.isEmpty()) {
        if (!completionText.startsWith(typedSinceRequest)) {
            LOG_MESSAGE("Completion no longer matches the typed text");
            return;
        }
        completionText = completionText.mid(typedSinceRequest.size());
    }

    if (completionText.trimmed().isEmpty()) {
        LOG_MESSAGE("No valid completions received");
        return;
    }

    const int anchorPosition = typedSinceRequest.isEmpty() ? requestPosition : currentPosition;
    const Text::Position anchor
        = Text::Position::fromPositionInDocument(editor->document(), anchorPosition);

    const TextSuggestion::Data suggestion{Text::Range{anchor, anchor}, anchor, completionText};
    editor->insertSuggestion(
        std::make_unique<LLMSuggestion>(QList<TextSuggestion::Data>{suggestion}, editor->document()));
}

void CodeCompletionController::cancelRunningRequest(TextEditor::TextEditorWidget *editor)
{
    const auto it = m_runningRequests.constFind(editor);
    if (it == m_runningRequests.constEnd())
        return;
    const quint64 requestId = it->id;
    CompletionEngine *engine = it->engine;
    m_runningRequests.erase(it);
    m_requestEditors.remove(requestId);
    m_progressHandler.hideProgress();
    if (engine)
        engine->cancel(requestId);
}

bool CodeCompletionController::isEnabled(ProjectExplorer::Project *project) const
{
    if (!project)
        return Settings::generalSettings().enableQodeAssist();

    Settings::ProjectSettings settings(project);
    return settings.isEnabled();
}

CompletionEngine *CodeCompletionController::activeEngine() const
{
    const QString mode = Settings::codeCompletionSettings().completionMode.stringValue();
    if (mode == QLatin1StringView(Constants::CC_MODE_AGENTIC_LOCAL))
        return m_agenticEngine;
    if (mode == QLatin1StringView(Constants::CC_MODE_AGENTIC_ACP))
        return m_acpEngine;
    return m_fimEngine;
}

void CodeCompletionController::setupConnections()
{
    for (CompletionEngine *engine : {m_fimEngine, m_agenticEngine, m_acpEngine}) {
        if (!engine)
            continue;
        connect(
            engine,
            &CompletionEngine::completionReady,
            this,
            &CodeCompletionController::handleCompletionReady);
        connect(
            engine,
            &CompletionEngine::completionFailed,
            this,
            &CodeCompletionController::handleCompletionFailed);
    }

    auto openDoc = [this](IDocument *document) {
        if (auto *textDocument = qobject_cast<TextDocument *>(document))
            openDocument(textDocument);
    };

    m_documentOpenedConnection
        = connect(EditorManager::instance(), &EditorManager::documentOpened, this, openDoc);
    m_documentClosedConnection = connect(
        EditorManager::instance(), &EditorManager::documentClosed, this, [this](IDocument *document) {
            if (auto textDocument = qobject_cast<TextDocument *>(document))
                closeDocument(textDocument);
        });

    for (IDocument *doc : DocumentModel::openedDocuments())
        openDoc(doc);
}

void CodeCompletionController::cleanupConnections()
{
    disconnect(m_documentOpenedConnection);
    disconnect(m_documentClosedConnection);

    for (auto it = m_documentConnections.begin(); it != m_documentConnections.end(); ++it)
        disconnect(it.value());
    m_documentConnections.clear();

    for (const auto &widget : std::as_const(m_hoverHandlerWidgets)) {
        if (widget)
            widget->removeHoverHandler(m_refactorHoverHandler);
    }
    m_hoverHandlerWidgets.clear();

    qDeleteAll(m_scheduledRequests);
    m_scheduledRequests.clear();
}

void CodeCompletionController::handleRefactoringResult(const RefactorResult &result)
{
    m_progressHandler.hideProgress();

    if (!result.success) {
        QString errorMessage = result.errorMessage.isEmpty()
                                   ? tr("Quick refactor failed")
                                   : tr("Quick refactor failed: %1").arg(result.errorMessage);

        if (result.editor) {
            m_errorHandler.showError(result.editor, errorMessage);
        }

        LOG_MESSAGE(QString("Refactoring failed: %1").arg(result.errorMessage));
        return;
    }

    if (!result.editor) {
        LOG_MESSAGE("Refactoring result has no editor");
        return;
    }

    int displayMode = Settings::quickRefactorSettings().displayMode();

    if (displayMode == 0) {
        displayRefactoringWidget(result);
    } else {
        displayRefactoringSuggestion(result);
    }
}

void CodeCompletionController::displayRefactoringSuggestion(const RefactorResult &result)
{
    TextEditorWidget *editorWidget = result.editor;

    Utils::Text::Range range{result.insertRange.begin, result.insertRange.end};
    Utils::Text::Position pos = result.insertRange.begin;

    int startPos = range.begin.toPositionInDocument(editorWidget->document());
    int endPos = range.end.toPositionInDocument(editorWidget->document());

    if (startPos != endPos) {
        QTextCursor startCursor(editorWidget->document());
        startCursor.setPosition(startPos);
        if (startCursor.positionInBlock() > 0) {
            startCursor.movePosition(QTextCursor::StartOfBlock);
        }

        QTextCursor endCursor(editorWidget->document());
        endCursor.setPosition(endPos);
        if (endCursor.positionInBlock() > 0) {
            endCursor.movePosition(QTextCursor::EndOfBlock);
            if (!endCursor.atEnd()) {
                endCursor.movePosition(QTextCursor::NextCharacter);
            }
        }

        Utils::Text::Position expandedBegin = Utils::Text::Position::fromPositionInDocument(
            editorWidget->document(), startCursor.position());
        Utils::Text::Position expandedEnd = Utils::Text::Position::fromPositionInDocument(
            editorWidget->document(), endCursor.position());

        range = Utils::Text::Range(expandedBegin, expandedEnd);
    }

    TextEditor::TextSuggestion::Data suggestionData{
        Utils::Text::Range{result.insertRange.begin, result.insertRange.end}, pos, result.newText};
    editorWidget->insertSuggestion(
        std::make_unique<RefactorSuggestion>(suggestionData, editorWidget->document()));

    m_refactorHoverHandler->setSuggestionRange(range);

    m_refactorHoverHandler->setApplyCallback([this, editorWidget]() {
        QKeyEvent tabEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QApplication::sendEvent(editorWidget, &tabEvent);
        m_refactorHoverHandler->clearSuggestionRange();
    });

    m_refactorHoverHandler->setDismissCallback([this, editorWidget]() {
        editorWidget->clearSuggestion();
        m_refactorHoverHandler->clearSuggestionRange();
    });

    LOG_MESSAGE("Displaying refactoring suggestion with hover handler");
}

void CodeCompletionController::displayRefactoringWidget(const RefactorResult &result)
{
    TextEditorWidget *editorWidget = result.editor;
    Utils::Text::Range range{result.insertRange.begin, result.insertRange.end};

    RefactorContext ctx = RefactorContextHelper::extractContext(editorWidget, range);

    QString displayOriginal;
    QString displayRefactored;
    QString textToApply = result.newText;

    if (ctx.isInsertion) {
        bool isMultiline = result.newText.contains('\n');

        if (isMultiline) {
            displayOriginal = ctx.textBeforeCursor;
            displayRefactored = ctx.textBeforeCursor + result.newText;
        } else {
            displayOriginal = ctx.textBeforeCursor + ctx.textAfterCursor;
            displayRefactored = ctx.textBeforeCursor + result.newText + ctx.textAfterCursor;
        }

        if (!ctx.textBeforeCursor.isEmpty() || !ctx.textAfterCursor.isEmpty()) {
            textToApply = result.newText;
        }
    } else {
        displayOriginal = ctx.originalText;
        displayRefactored = result.newText;
    }

    m_refactorWidgetHandler->setApplyCallback([this, editorWidget, result](const QString &editedText) {
        applyRefactoringEdit(editorWidget, result.insertRange, editedText);
    });

    m_refactorWidgetHandler->setDeclineCallback([]() {});

    m_refactorWidgetHandler->showRefactorWidget(
        editorWidget, displayOriginal, displayRefactored, range,
        ctx.contextBefore, ctx.contextAfter);

    m_refactorWidgetHandler->setTextToApply(textToApply);
}

void CodeCompletionController::applyRefactoringEdit(
    TextEditor::TextEditorWidget *editor, const Utils::Text::Range &range, const QString &text)
{
    const QTextCursor startCursor = range.begin.toTextCursor(editor->document());
    const QTextCursor endCursor = range.end.toTextCursor(editor->document());
    const int startPos = startCursor.position();
    const int endPos = endCursor.position();

    QTextCursor editCursor(editor->document());
    editCursor.beginEditBlock();

    if (startPos == endPos) {
        bool isMultiline = text.contains('\n');
        editCursor.setPosition(startPos);

        if (isMultiline) {
            editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            editCursor.removeSelectedText();
        }

        editCursor.insertText(text);
    } else {
        editCursor.setPosition(startPos);
        editCursor.setPosition(endPos, QTextCursor::KeepAnchor);
        editCursor.removeSelectedText();
        editCursor.insertText(text);
    }

    editCursor.endEditBlock();
}

void CodeCompletionController::handleAutoRequestTrigger(TextEditor::TextEditorWidget *widget)
{
    const QTextCursor cursor = widget->textCursor();
    const auto &settings = Settings::codeCompletionSettings();
    const bool smart = settings.smartContextTrigger();

    if (smart && (isInsideIdentifier(cursor) || isAfterMemberAccess(cursor)))
        return;

    const bool eager = smart && (isFreshIndentedLine(cursor) || isAfterEagerTrigger(cursor));
    const int charThreshold = settings.autoCompletionCharThreshold();

    if (eager || m_recentCharCount > charThreshold)
        scheduleRequest(widget);
}

bool CodeCompletionController::eventFilter(QObject *watched, QEvent *event)
{
    auto *editor = qobject_cast<TextEditor::TextEditorWidget *>(watched);
    if (!editor)
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Escape) {
            if (m_runningRequests.contains(editor)) {
                cancelRunningRequest(editor);
            }

            if (m_scheduledRequests.contains(editor)) {
                auto *timer = m_scheduledRequests.value(editor);
                if (timer && timer->isActive()) {
                    timer->stop();
                }
            }

            if (m_refactorHandler && m_refactorHandler->isProcessing()) {
                m_refactorHandler->cancelRequest();
            }

            m_progressHandler.hideProgress();
        }
    }

    return QObject::eventFilter(watched, event);
}

} // namespace QodeAssist
