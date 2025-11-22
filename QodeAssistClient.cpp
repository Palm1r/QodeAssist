/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024-2025 Petr Mironychev
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
 */

#include "QodeAssistClient.hpp"

#include <QApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QTimer>

#include <coreplugin/icore.h>
#include <languageclient/languageclientsettings.h>
#include <projectexplorer/projectmanager.h>

#include "LLMClientInterface.hpp"
#include "LLMSuggestion.hpp"
#include "RefactorSuggestion.hpp"
#include "RefactorSuggestionHoverHandler.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProjectSettings.hpp"
#include "settings/QuickRefactorSettings.hpp"
#include "widgets/RefactorWidgetHandler.hpp"
#include <context/ChangesManager.h>
#include <logger/Logger.hpp>

using namespace LanguageServerProtocol;
using namespace TextEditor;
using namespace Utils;
using namespace ProjectExplorer;
using namespace Core;

namespace QodeAssist {

QodeAssistClient::QodeAssistClient(LLMClientInterface *clientInterface)
    : LanguageClient::Client(clientInterface)
    , m_llmClient(clientInterface)
    , m_recentCharCount(0)
{
    setName("QodeAssist");
    LanguageClient::LanguageFilter filter;
    filter.mimeTypes = QStringList() << "*";
    setSupportedLanguage(filter);

    start();
    setupConnections();

    m_typingTimer.start();

    m_hintHideTimer.setSingleShot(true);
    m_hintHideTimer.setInterval(Settings::codeCompletionSettings().hintHideTimeout());
    connect(&m_hintHideTimer, &QTimer::timeout, this, [this]() { m_hintHandler.hideHint(); });

    m_refactorHoverHandler = new RefactorSuggestionHoverHandler();
    m_refactorWidgetHandler = new RefactorWidgetHandler(this);
}

QodeAssistClient::~QodeAssistClient()
{
    cleanupConnections();
    delete m_refactorHoverHandler;
    delete m_refactorWidgetHandler;
}

void QodeAssistClient::openDocument(TextEditor::TextDocument *document)
{
    auto project = ProjectManager::projectForFile(document->filePath());
    if (!isEnabled(project))
        return;

    Client::openDocument(document);

    auto editors = TextEditor::BaseTextEditor::textEditorsForDocument(document);
    for (auto *editor : editors) {
        if (auto *widget = editor->editorWidget()) {
            widget->addHoverHandler(m_refactorHoverHandler);
            widget->installEventFilter(this);
        }
    }

    connect(
        document,
        &TextDocument::contentsChangedWithPosition,
        this,
        [this, document](int position, int charsRemoved, int charsAdded) {
            if (!Settings::codeCompletionSettings().autoCompletion())
                return;

            auto project = ProjectManager::projectForFile(document->filePath());
            if (!isEnabled(project))
                return;

            auto textEditor = BaseTextEditor::currentTextEditor();
            if (!textEditor || textEditor->document() != document)
                return;

            if (Settings::codeCompletionSettings().useProjectChangesCache())
                Context::ChangesManager::instance()
                    .addChange(document, position, charsRemoved, charsAdded);

            TextEditorWidget *widget = textEditor->editorWidget();
            if (widget->isReadOnly() || widget->multiTextCursor().hasMultipleCursors())
                return;

            const int cursorPosition = widget->textCursor().position();
            if (cursorPosition < position || cursorPosition > position + charsAdded)
                return;

            if (charsRemoved > 0 || charsAdded <= 0) {
                m_recentCharCount = 0;
                m_typingTimer.restart();
                // 0 = Hint-based, 1 = Automatic
                const int triggerMode = Settings::codeCompletionSettings().completionTriggerMode();
                if (triggerMode != 1) {
                    m_hintHideTimer.stop();
                    m_hintHandler.hideHint();
                }
                return;
            }

            QTextCursor cursor = widget->textCursor();
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
            QString lastChar = cursor.selectedText();

            if (lastChar.isEmpty() || lastChar[0].isPunct()) {
                m_recentCharCount = 0;
                m_typingTimer.restart();
                // 0 = Hint-based, 1 = Automatic
                const int triggerMode = Settings::codeCompletionSettings().completionTriggerMode();
                if (triggerMode != 1) {
                    m_hintHideTimer.stop();
                    m_hintHandler.hideHint();
                }
                return;
            }

            bool isSpaceOrTab = lastChar[0].isSpace();
            bool ignoreWhitespace
                = Settings::codeCompletionSettings().ignoreWhitespaceInCharCount();

            if (!ignoreWhitespace || !isSpaceOrTab) {
                m_recentCharCount += charsAdded;
            }

            if (m_typingTimer.elapsed()
                > Settings::codeCompletionSettings().autoCompletionTypingInterval()) {
                m_recentCharCount = (ignoreWhitespace && isSpaceOrTab) ? 0 : charsAdded;
                m_typingTimer.restart();
            }

            // 0 = Hint-based, 1 = Automatic
            const int triggerMode = Settings::codeCompletionSettings().completionTriggerMode();
            if (triggerMode == 1) {
                handleAutoRequestTrigger(widget, charsAdded, isSpaceOrTab);
            } else {
                handleHintBasedTrigger(widget, charsAdded, isSpaceOrTab, cursor);
            }
        });
}

bool QodeAssistClient::canOpenProject(ProjectExplorer::Project *project)
{
    return isEnabled(project);
}

void QodeAssistClient::requestCompletions(TextEditor::TextEditorWidget *editor)
{
    auto project = ProjectManager::projectForFile(editor->textDocument()->filePath());

    if (!isEnabled(project))
        return;


    if (m_llmClient->contextManager()
            ->ignoreManager()
            ->shouldIgnore(editor->textDocument()->filePath().toUrlishString(), project)) {
        LOG_MESSAGE(QString("Ignoring file due to .qodeassistignore: %1")
                        .arg(editor->textDocument()->filePath().toUrlishString()));
        return;
    }

    MultiTextCursor cursor = editor->multiTextCursor();
    if (cursor.hasMultipleCursors() || cursor.hasSelection() || editor->suggestionVisible())
        return;

    const int triggerMode = Settings::codeCompletionSettings().completionTriggerMode();

    if (Settings::codeCompletionSettings().abortAssistOnRequest() && triggerMode == 0) {
        editor->abortAssist();
    }

    const FilePath filePath = editor->textDocument()->filePath();
    GetCompletionRequest request{
        {TextDocumentIdentifier(hostPathToServerUri(filePath)),
         documentVersion(filePath),
         Position(cursor.mainCursor())}};
    if (Settings::codeCompletionSettings().showProgressWidget()) {
        m_progressHandler.setCancelCallback([this, editor = QPointer<TextEditorWidget>(editor)]() {
            if (editor) {
                cancelRunningRequest(editor);
            }
        });
        m_progressHandler.showProgress(editor);
    }
    request.setResponseCallback([this, editor = QPointer<TextEditorWidget>(editor)](
                                    const GetCompletionRequest::Response &response) {
        QTC_ASSERT(editor, return);
        handleCompletions(response, editor);
    });
    m_runningRequests[editor] = request;
    sendMessage(request);
}

void QodeAssistClient::requestQuickRefactor(
    TextEditor::TextEditorWidget *editor, const QString &instructions)
{
    auto project = ProjectManager::projectForFile(editor->textDocument()->filePath());

    if (!isEnabled(project))
        return;

    if (m_llmClient->contextManager()
            ->ignoreManager()
            ->shouldIgnore(editor->textDocument()->filePath().toUrlishString(), project)) {
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
            &QodeAssistClient::handleRefactoringResult);
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

void QodeAssistClient::scheduleRequest(TextEditor::TextEditorWidget *editor)
{
    cancelRunningRequest(editor);

    auto it = m_scheduledRequests.find(editor);
    if (it == m_scheduledRequests.end()) {
        auto timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, editor]() {
            if (editor
                && editor->textCursor().position()
                       == m_scheduledRequests[editor]->property("cursorPosition").toInt()
                && m_recentCharCount
                       > Settings::codeCompletionSettings().autoCompletionCharThreshold())
                requestCompletions(editor);
        });
        connect(editor, &TextEditorWidget::destroyed, this, [this, editor]() {
            delete m_scheduledRequests.take(editor);
            cancelRunningRequest(editor);
        });
        connect(editor, &TextEditorWidget::cursorPositionChanged, this, [this, editor] {
            cancelRunningRequest(editor);
            // 0 = Hint-based, 1 = Automatic
            const int triggerMode = Settings::codeCompletionSettings().completionTriggerMode();
            if (triggerMode != 1) {
                m_hintHideTimer.stop();
                m_hintHandler.hideHint();
            }
        });
        it = m_scheduledRequests.insert(editor, timer);
    }

    it.value()->setProperty("cursorPosition", editor->textCursor().position());
    it.value()->start(Settings::codeCompletionSettings().startSuggestionTimer());
}
void QodeAssistClient::handleCompletions(
    const GetCompletionRequest::Response &response, TextEditor::TextEditorWidget *editor)
{
    m_progressHandler.hideProgress();
    const int triggerMode = Settings::codeCompletionSettings().completionTriggerMode();

    if (Settings::codeCompletionSettings().abortAssistOnRequest() && triggerMode == 1) {
        editor->abortAssist();
    }

    if (response.error()) {
        log(*response.error());
        m_errorHandler
            .showError(editor, tr("Code completion failed: %1").arg(response.error()->message()));
        return;
    }

    int requestPosition = -1;
    if (const auto requestParams = m_runningRequests.take(editor).params())
        requestPosition = requestParams->position().toPositionInDocument(editor->document());

    const MultiTextCursor cursors = editor->multiTextCursor();
    if (cursors.hasMultipleCursors())
        return;

    if (cursors.hasSelection() || cursors.mainCursor().position() != requestPosition)
        return;

    if (const std::optional<GetCompletionResponse> result = response.result()) {
        auto isValidCompletion = [](const Completion &completion) {
            return completion.isValid() && !completion.text().trimmed().isEmpty();
        };
        QList<Completion> completions
            = Utils::filtered(result->completions().toListOrEmpty(), isValidCompletion);

        for (Completion &completion : completions) {
            const LanguageServerProtocol::Range range = completion.range();
            if (range.start().line() != range.end().line())
                continue;

            const QString completionText = completion.text();
            const int end = int(completionText.size()) - 1;
            int delta = 0;
            while (delta <= end && completionText[end - delta].isSpace())
                ++delta;

            if (delta > 0)
                completion.setText(completionText.chopped(delta));
        }
        auto suggestions = Utils::transform(completions, [](const Completion &c) {
            auto toTextPos = [](const LanguageServerProtocol::Position pos) {
                return Text::Position{pos.line() + 1, pos.character()};
            };

            Text::Range range{toTextPos(c.range().start()), toTextPos(c.range().end())};
            Text::Position pos{toTextPos(c.position())};
            return TextSuggestion::Data{range, pos, c.text()};
        });

        if (completions.isEmpty()) {
            LOG_MESSAGE("No valid completions received");
            return;
        }
        editor->insertSuggestion(std::make_unique<LLMSuggestion>(suggestions, editor->document()));
    }
}

void QodeAssistClient::cancelRunningRequest(TextEditor::TextEditorWidget *editor)
{
    const auto it = m_runningRequests.constFind(editor);
    if (it == m_runningRequests.constEnd())
        return;
    m_progressHandler.hideProgress();
    // 0 = Hint-based, 1 = Automatic
    const int triggerMode = Settings::codeCompletionSettings().completionTriggerMode();
    if (triggerMode != 1) {
        m_hintHideTimer.stop();
        m_hintHandler.hideHint();
    }
    cancelRequest(it->id());
    m_runningRequests.erase(it);
}

bool QodeAssistClient::isEnabled(ProjectExplorer::Project *project) const
{
    if (!project)
        return Settings::generalSettings().enableQodeAssist();

    Settings::ProjectSettings settings(project);
    return settings.isEnabled();
}

void QodeAssistClient::setupConnections()
{
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

void QodeAssistClient::cleanupConnections()
{
    disconnect(m_documentOpenedConnection);
    disconnect(m_documentClosedConnection);

    qDeleteAll(m_scheduledRequests);
    m_scheduledRequests.clear();
}

bool QodeAssistClient::isHintVisible() const
{
    return m_hintHandler.isHintVisible();
}

void QodeAssistClient::hideHintAndRequestCompletion(TextEditor::TextEditorWidget *editor)
{
    m_hintHandler.hideHint();
    requestCompletions(editor);
}

void QodeAssistClient::handleRefactoringResult(const RefactorResult &result)
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

namespace {
Utils::Text::Position toTextPos(const Utils::Text::Position &pos)
{
    return Utils::Text::Position{pos.line, pos.column};
}
} // anonymous namespace

void QodeAssistClient::displayRefactoringSuggestion(const RefactorResult &result)
{
    TextEditorWidget *editorWidget = result.editor;

    Utils::Text::Range range{toTextPos(result.insertRange.begin), toTextPos(result.insertRange.end)};
    Utils::Text::Position pos = toTextPos(result.insertRange.begin);

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
        Utils::Text::Range{toTextPos(result.insertRange.begin), toTextPos(result.insertRange.end)},
        pos,
        result.newText};
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

void QodeAssistClient::displayRefactoringWidget(const RefactorResult &result)
{
    TextEditorWidget *editorWidget = result.editor;

    Utils::Text::Range range{toTextPos(result.insertRange.begin), toTextPos(result.insertRange.end)};

    QString originalText;
    const int startPos = range.begin.toPositionInDocument(editorWidget->document());
    const int endPos = range.end.toPositionInDocument(editorWidget->document());
    
    if (startPos != endPos) {
        QTextCursor cursor(editorWidget->document());
        cursor.setPosition(startPos);
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);
        originalText = cursor.selectedText();
        originalText.replace(QChar(0x2029), "\n");
    }

    m_refactorWidgetHandler->setApplyCallback([this, editorWidget, result](const QString &editedText) {
        const Utils::Text::Range range{
            Utils::Text::Position{result.insertRange.begin.line, result.insertRange.begin.column},
            Utils::Text::Position{result.insertRange.end.line, result.insertRange.end.column}};

        const QTextCursor startCursor = range.begin.toTextCursor(editorWidget->document());
        const QTextCursor endCursor = range.end.toTextCursor(editorWidget->document());
        
        const int startPos = startCursor.position();
        const int endPos = endCursor.position();
        
        QTextCursor editCursor(editorWidget->document());
        editCursor.beginEditBlock();

        if (startPos == endPos) {
            editCursor.setPosition(startPos);
            editCursor.insertText(editedText);
        } else {
            editCursor.setPosition(startPos);
            editCursor.setPosition(endPos, QTextCursor::KeepAnchor);
            editCursor.removeSelectedText();
            editCursor.insertText(editedText);
        }

        editCursor.endEditBlock();
        
        LOG_MESSAGE("Refactoring applied via widget with edited text");
    });

    m_refactorWidgetHandler->setDeclineCallback([this]() {
        LOG_MESSAGE("Refactoring declined via widget");
    });

    m_refactorWidgetHandler->showRefactorWidget(
        editorWidget, 
        originalText, 
        result.newText, 
        range);
    
    LOG_MESSAGE(QString("Displaying refactoring widget - Original: %1 chars, New: %2 chars")
        .arg(originalText.length())
        .arg(result.newText.length()));
}

void QodeAssistClient::handleAutoRequestTrigger(TextEditor::TextEditorWidget *widget,
                                                 int charsAdded,
                                                 bool isSpaceOrTab)
{
    Q_UNUSED(isSpaceOrTab);

    if (m_recentCharCount
        > Settings::codeCompletionSettings().autoCompletionCharThreshold()) {
        scheduleRequest(widget);
    }
}

void QodeAssistClient::handleHintBasedTrigger(TextEditor::TextEditorWidget *widget,
                                               int charsAdded,
                                               bool isSpaceOrTab,
                                               QTextCursor &cursor)
{
    Q_UNUSED(charsAdded);

    const int hintThreshold = Settings::codeCompletionSettings().hintCharThreshold();
    if (m_recentCharCount >= hintThreshold && !isSpaceOrTab) {
        const QRect cursorRect = widget->cursorRect(cursor);
        QPoint globalPos = widget->viewport()->mapToGlobal(cursorRect.topLeft());
        QPoint localPos = widget->mapFromGlobal(globalPos);

        int fontSize = widget->font().pixelSize();
        if (fontSize <= 0) {
            fontSize = widget->fontMetrics().height();
        }

        QTextCursor textCursor = widget->textCursor();

        if (m_recentCharCount <= hintThreshold) {
            textCursor
                .movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, m_recentCharCount);
        } else {
            textCursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, hintThreshold);
        }

        int x = localPos.x() + cursorRect.height();
        int y = localPos.y() + cursorRect.height() / 4;

        QPoint hintPos(x, y);

        if (!m_hintHandler.isHintVisible()) {
            m_hintHandler.showHint(widget, hintPos, fontSize);
        } else {
            m_hintHandler.updateHintPosition(widget, hintPos);
        }

        m_hintHideTimer.start();
    }
}

bool QodeAssistClient::eventFilter(QObject *watched, QEvent *event)
{
    auto *editor = qobject_cast<TextEditor::TextEditorWidget *>(watched);
    if (!editor)
        return LanguageClient::Client::eventFilter(watched, event);

    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        // Check hint trigger key (0=Space, 1=Ctrl+Space, 2=Alt+Space, 3=Ctrl+Enter, 4=Tab, 5=Enter)
        if (m_hintHandler.isHintVisible()) {
            const int triggerKeyIndex = Settings::codeCompletionSettings().hintTriggerKey();
            bool isMatchingKey = false;
            const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();

            switch (triggerKeyIndex) {
            case 0: // Space
                isMatchingKey = (keyEvent->key() == Qt::Key_Space 
                                 && (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier));
                break;
            case 1: // Ctrl+Space
                isMatchingKey = (keyEvent->key() == Qt::Key_Space 
                                 && (modifiers & Qt::ControlModifier));
                break;
            case 2: // Alt+Space
                isMatchingKey = (keyEvent->key() == Qt::Key_Space 
                                 && (modifiers & Qt::AltModifier));
                break;
            case 3: // Ctrl+Enter
                isMatchingKey = ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                                 && (modifiers & Qt::ControlModifier));
                break;
            case 4: // Tab
                isMatchingKey = (keyEvent->key() == Qt::Key_Tab);
                break;
            case 5: // Enter
                isMatchingKey = ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                                 && (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier));
                break;
            }

            if (isMatchingKey) {
                m_hintHideTimer.stop();
                m_hintHandler.hideHint();
                requestCompletions(editor);
                return true;
            }
        }

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
            m_hintHideTimer.stop();
            m_hintHandler.hideHint();
        }
    }

    return LanguageClient::Client::eventFilter(watched, event);
}

} // namespace QodeAssist
