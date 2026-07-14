// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatEditor.hpp"

#include <coreplugin/editormanager/editormanager.h>

#include "ChatDocument.hpp"
#include "ChatView/ChatRootView.hpp"
#include "ChatView/ChatWidget.hpp"
#include "plugin/QodeAssistConstants.hpp"
#include "plugin/QodeAssisttr.h"

namespace QodeAssist::Chat {

ChatEditor::ChatEditor(
    QQmlEngine *engine,
    SessionFileRegistry *sessionFileRegistry,
    Skills::SkillsManager *skillsManager)
    : m_engine(engine)
    , m_sessionFileRegistry(sessionFileRegistry)
    , m_skillsManager(skillsManager)
    , m_document(new ChatDocument(this))
    , m_chatWidget(new ChatWidget(engine, sessionFileRegistry, skillsManager, false))
{
    setWidget(m_chatWidget);
    setContext(Core::Context(Constants::QODE_ASSIST_CHAT_CONTEXT));
    setDuplicateSupported(false);

    if (auto rootView = qobject_cast<ChatRootView *>(m_chatWidget->rootObject())) {
        rootView->setInEditor(true);
        connect(
            rootView,
            &ChatRootView::closeHostRequested,
            this,
            [this] { Core::EditorManager::closeEditors({this}); },
            Qt::QueuedConnection);

        auto syncTitle = [this, rootView] {
            const QString title = rootView->chatTitle();
            m_document->setPreferredDisplayName(
                title.isEmpty() ? Tr::tr("QodeAssist Chat") : QStringLiteral("QodeAssist - ") + title);
        };
        connect(rootView, &ChatRootView::chatTitleChanged, this, syncTitle);
        syncTitle();
    }
}

void ChatEditor::consumePendingChatFile()
{
    if (auto rootView = qobject_cast<ChatRootView *>(m_chatWidget->rootObject()))
        rootView->consumePendingChatFile();
}

ChatEditor::~ChatEditor()
{
    delete m_chatWidget;
}

Core::IDocument *ChatEditor::document() const
{
    return m_document;
}

QWidget *ChatEditor::toolBar()
{
    return nullptr;
}

Core::IEditor *ChatEditor::duplicate()
{
    return nullptr;
}

} // namespace QodeAssist::Chat
