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

    m_refactorHoverHandler = new RefactorSuggestionHoverHandler();
}

QodeAssistClient::~QodeAssistClient()
{
    cleanupConnections();
    delete m_refactorHoverHandler;
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
                return;
            }

            QTextCursor cursor = widget->textCursor();
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 1);
            QString lastChar = cursor.selectedText();

            if (lastChar.isEmpty() || lastChar[0].isPunct()) {
                m_recentCharCount = 0;
                m_typingTimer.restart();
                return;
            }

            m_recentCharCount += charsAdded;

            if (m_typingTimer.elapsed()
                > Settings::codeCompletionSettings().autoCompletionTypingInterval()) {
                m_recentCharCount = charsAdded;
                m_typingTimer.restart();
            }

            if (m_recentCharCount
                > Settings::codeCompletionSettings().autoCompletionCharThreshold()) {
                scheduleRequest(widget);
            }
        });

    // auto editors = BaseTextEditor::textEditorsForDocument(document);
    // connect(
    //     editors.first()->editorWidget(),
    //     &TextEditorWidget::selectionChanged,
    //     this,
    //     [this, editors]() { m_chatButtonHandler.showButton(editors.first()->editorWidget()); });
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

    const FilePath filePath = editor->textDocument()->filePath();
    GetCompletionRequest request{
        {TextDocumentIdentifier(hostPathToServerUri(filePath)),
         documentVersion(filePath),
         Position(cursor.mainCursor())}};
    if (Settings::codeCompletionSettings().showProgressWidget()) {
        // Setup cancel callback before showing progress
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

    // Setup cancel callback before showing progress
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

    if (response.error()) {
        log(*response.error());

        QString errorMessage = tr("Code completion failed: %1").arg(response.error()->message());
        m_errorHandler.showError(editor, errorMessage);
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

        // remove trailing whitespaces from the end of the completions
        for (Completion &completion : completions) {
            const LanguageServerProtocol::Range range = completion.range();
            if (range.start().line() != range.end().line())
                continue; // do not remove trailing whitespaces for multi-line replacements

            const QString completionText = completion.text();
            const int end = int(completionText.size()) - 1; // empty strings have been removed above
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

void QodeAssistClient::handleRefactoringResult(const RefactorResult &result)
{
    m_progressHandler.hideProgress();

    if (!result.success) {
        // Show error to user
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

    TextEditorWidget *editorWidget = result.editor;

    auto toTextPos = [](const Utils::Text::Position &pos) {
        return Utils::Text::Position{pos.line, pos.column};
    };

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

bool QodeAssistClient::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Escape) {
            auto *editor = qobject_cast<TextEditor::TextEditorWidget *>(watched);

            if (editor) {
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
    }

    return LanguageClient::Client::eventFilter(watched, event);
}

} // namespace QodeAssist
