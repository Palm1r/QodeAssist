/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of Qode Assist.
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

#include <QTimer>

#include <languageclient/languageclientsettings.h>
#include <projectexplorer/projectmanager.h>

#include "LLMClientInterface.hpp"
#include "LLMSuggestion.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ProjectSettings.hpp"
#include <context/ChangesManager.h>

using namespace LanguageServerProtocol;
using namespace TextEditor;
using namespace Utils;
using namespace ProjectExplorer;
using namespace Core;

namespace QodeAssist {

QodeAssistClient::QodeAssistClient(LLMClientInterface *clientInterface)
    : LanguageClient::Client(clientInterface)
    , m_recentCharCount(0)
{
    setName("Qode Assist");
    LanguageClient::LanguageFilter filter;
    filter.mimeTypes = QStringList() << "*";
    setSupportedLanguage(filter);

    start();
    setupConnections();

    m_typingTimer.start();
}

QodeAssistClient::~QodeAssistClient()
{
    cleanupConnections();
}

void QodeAssistClient::openDocument(TextEditor::TextDocument *document)
{
    auto project = ProjectManager::projectForFile(document->filePath());
    if (!isEnabled(project))
        return;

    Client::openDocument(document);
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

    MultiTextCursor cursor = editor->multiTextCursor();
    if (cursor.hasMultipleCursors() || cursor.hasSelection() || editor->suggestionVisible())
        return;

    const FilePath filePath = editor->textDocument()->filePath();
    GetCompletionRequest request{
        {TextDocumentIdentifier(hostPathToServerUri(filePath)),
         documentVersion(filePath),
         Position(cursor.mainCursor())}};
    request.setResponseCallback([this, editor = QPointer<TextEditorWidget>(editor)](
                                    const GetCompletionRequest::Response &response) {
        QTC_ASSERT(editor, return);
        handleCompletions(response, editor);
    });
    m_runningRequests[editor] = request;
    sendMessage(request);
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
    if (response.error())
        log(*response.error());

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
        if (completions.isEmpty())
            return;
        editor->insertSuggestion(std::make_unique<LLMSuggestion>(suggestions, editor->document()));
    }
}

void QodeAssistClient::cancelRunningRequest(TextEditor::TextEditorWidget *editor)
{
    const auto it = m_runningRequests.constFind(editor);
    if (it == m_runningRequests.constEnd())
        return;
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

} // namespace QodeAssist
