// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatEditor.hpp"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>

#include <QAction>

#include "ChatDocument.hpp"
#include "ChatView/ChatRootView.hpp"
#include "ChatView/ChatWidget.hpp"
#include "QodeAssistConstants.hpp"

namespace QodeAssist::Chat {

ChatEditor::ChatEditor(
    QQmlEngine *engine,
    SessionFileRegistry *sessionFileRegistry,
    Skills::SkillsManager *skillsManager)
    : m_engine(engine)
    , m_sessionFileRegistry(sessionFileRegistry)
    , m_skillsManager(skillsManager)
    , m_document(new ChatDocument(this))
    , m_chatWidget(new ChatWidget(engine, sessionFileRegistry, skillsManager))
{
    setWidget(m_chatWidget);
    setContext(Core::Context(Constants::QODE_ASSIST_CHAT_CONTEXT));
    setDuplicateSupported(true);

    if (auto rootView = qobject_cast<ChatRootView *>(m_chatWidget->rootObject())) {
        connect(
            rootView,
            &ChatRootView::closeHostRequested,
            this,
            [this] {
                Core::EditorManager::closeEditors({this});
                if (auto command
                    = Core::ActionManager::command(Core::Constants::REMOVE_CURRENT_SPLIT)) {
                    if (auto action = command->action(); action && action->isEnabled())
                        action->trigger();
                }
            },
            Qt::QueuedConnection);
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
    return new ChatEditor(m_engine, m_sessionFileRegistry, m_skillsManager);
}

} // namespace QodeAssist::Chat
